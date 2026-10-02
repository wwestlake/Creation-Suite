#include "node_system/graph_nodes.h"

#include <algorithm>

namespace ce::node_system {

namespace {
bool SameType(const PinTypeDesc& a, const PinTypeDesc& b) {
    return a.kind == b.kind && a.dataType == b.dataType && a.enumType == b.enumType;
}

PinDefaultValue PortDefault(const PinTypeDesc& type) {
    // An unwired image input on a Graph node is a slot for a project image, like any image input.
    return type.dataType == DataType::Texture ? PinDefaultValue { std::string() } : DefaultValueFor(type.dataType);
}

// Brings one side of a Graph node's pins (inputs or outputs, beyond the fixed ones) in line with the ports.
void SyncSide(Graph& host, Node& node, bool inputs, const std::vector<InterfacePort>& ports) {
    const auto& pins = inputs ? node.Inputs() : node.Outputs();
    std::vector<PinId> remove;
    std::vector<std::string> kept;
    for (const auto& pin : pins) {
        if (inputs && pin.name == kGraphPathPin) {
            continue;
        }
        const auto match = std::find_if(ports.begin(), ports.end(),
                                        [&pin](const InterfacePort& p) { return p.name == pin.name && SameType(p.type, pin.type); });
        if (match == ports.end()) {
            remove.push_back(pin.id);
        } else {
            kept.push_back(pin.name);
        }
    }
    for (PinId id : remove) {
        host.DisconnectPin(node.Id(), id);
        node.RemovePin(id);
    }
    for (const auto& port : ports) {
        if (std::find(kept.begin(), kept.end(), port.name) != kept.end()) {
            continue;
        }
        if (inputs) {
            node.AddInput(port.name, port.type, port.defaultValue);
        } else {
            node.AddOutput(port.name, port.type, {});
        }
    }
}
} // namespace

std::string GraphPortName(const Node& node, GraphPort role) {
    for (const auto& pin : node.Inputs()) {
        if (pin.name == kGraphPortNamePin) {
            if (const auto* text = std::get_if<std::string>(&pin.defaultValue); text != nullptr && !text->empty()) {
                std::string name = *text;
                std::replace(name.begin(), name.end(), ' ', '_'); // pin names have no spaces (frgraph)
                return name;
            }
        }
    }
    return role == GraphPort::input ? "input" : "output";
}

GraphInterface InterfaceOf(const Graph& graph, const NodeTypeRegistry& registry) {
    GraphInterface result;
    for (const auto& symbol : graph.Symbols()) {
        if (symbol.kind != SymbolKind::Param) {
            continue;
        }
        InterfacePort port;
        port.name = symbol.id;
        port.label = symbol.name;
        port.type = { PinKind::Data, symbol.type };
        port.type.enumType = symbol.enumType;
        port.defaultValue = symbol.value;
        port.fromParam = true;
        result.inputs.push_back(std::move(port));
    }

    // Port nodes in id order, so the interface is stable.
    std::vector<NodeId> ids;
    for (const auto& [id, node] : graph.Nodes()) {
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    for (NodeId id : ids) {
        const Node& node = *graph.FindNode(id);
        const auto* descriptor = registry.Find(node.TypeName());
        if (descriptor == nullptr || descriptor->graphPort == GraphPort::none) {
            continue;
        }
        InterfacePort port;
        port.name = GraphPortName(node, descriptor->graphPort);
        port.label = port.name;
        if (descriptor->graphPort == GraphPort::input) {
            if (node.Outputs().empty()) {
                continue;
            }
            port.type = node.Outputs().front().type;
            port.defaultValue = PortDefault(port.type);
            auto& side = result.inputs;
            if (std::none_of(side.begin(), side.end(), [&port](const InterfacePort& p) { return p.name == port.name; })) {
                side.push_back(std::move(port));
            }
        } else {
            const auto value = std::find_if(node.Inputs().begin(), node.Inputs().end(),
                                            [](const Pin& p) { return p.name != kGraphPortNamePin; });
            if (value == node.Inputs().end()) {
                continue;
            }
            port.type = value->type;
            auto& side = result.outputs;
            if (std::none_of(side.begin(), side.end(), [&port](const InterfacePort& p) { return p.name == port.name; })) {
                side.push_back(std::move(port));
            }
        }
    }
    return result;
}

void RegisterGraphNode(NodeTypeRegistry& registry, std::vector<std::string> diagramTypes) {
    NodeTypeDescriptor d;
    d.typeName = kGraphNodeType;
    d.domain = Domain::Core;
    d.inputs = { { kGraphPathPin, { PinKind::Data, DataType::String }, std::string() } };
    d.displayName = "Graph";
    d.category = "Graphs";
    d.description = "Uses another graph as a node: choose the graph in Properties; its params and inputs become this node's "
                    "inputs and its outputs this node's outputs (shared/NodeSystem/GRAPH_TYPES.md).";
    d.diagramTypes = std::move(diagramTypes);
    d.dynamicPins = true;
    registry.Register(std::move(d));
}

bool IsGraphNode(const Node& node) {
    return node.TypeName() == kGraphNodeType;
}

std::string GraphNodePath(const Node& node) {
    for (const auto& pin : node.Inputs()) {
        if (pin.name == kGraphPathPin) {
            if (const auto* text = std::get_if<std::string>(&pin.defaultValue)) {
                return *text;
            }
        }
    }
    return {};
}

bool SyncGraphNodePins(Graph& host, NodeId graphNode, const GraphInterface& graphInterface) {
    Node* node = host.FindNode(graphNode);
    if (node == nullptr) {
        return false;
    }
    SyncSide(host, *node, true, graphInterface.inputs);
    SyncSide(host, *node, false, graphInterface.outputs);
    return true;
}

} // namespace ce::node_system
