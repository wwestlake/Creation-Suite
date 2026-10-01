#include "node_system/flow_nodes.h"

#include <algorithm>
#include <cmath>

namespace ce::node_system {

namespace {
std::string PinName(std::string name) {
    std::replace(name.begin(), name.end(), ' ', '_'); // pin names have no spaces (frgraph)
    return name;
}

const Pin* FindInput(const Node& node, const char* name) {
    for (const auto& pin : node.Inputs()) {
        if (pin.name == name) {
            return &pin;
        }
    }
    return nullptr;
}

bool IsFixedInput(FlowKind kind, const std::string& name) {
    return name == kFlowSelectorPin || name == kFlowCasesPin || (kind == FlowKind::route && name == kFlowValuePin);
}
} // namespace

FlowType StandardFlowType(DataType type) {
    switch (type) {
        case DataType::Texture: return { type, "image", "Image" };
        case DataType::Float: return { type, "number", "Number" };
        case DataType::Int: return { type, "integer", "Integer" };
        case DataType::Bool: return { type, "toggle", "Toggle" };
        case DataType::Color: return { type, "color", "Color" };
        case DataType::Vec3: return { type, "vector", "Vector" };
        case DataType::String: return { type, "text", "Text" };
        case DataType::Drawing: return { type, "drawing", "Drawing" };
        case DataType::Brush: return { type, "brush", "Brush" };
        default: return { type, "value", "Value" };
    }
}

void RegisterFlowNodes(NodeTypeRegistry& registry, const std::vector<FlowType>& types, std::vector<std::string> diagramTypes) {
    const PinTypeDesc any { PinKind::Data, DataType::Any };
    const PinTypeDesc integer { PinKind::Data, DataType::Int };
    for (const auto& flow : types) {
        const PinTypeDesc value { PinKind::Data, flow.type };

        NodeTypeDescriptor sw;
        sw.typeName = kSwitchPrefix + flow.suffix;
        sw.domain = Domain::Core;
        sw.inputs = { { kFlowSelectorPin, any, std::int64_t { 0 } }, { kFlowCasesPin, integer, std::int64_t { 2 } } };
        sw.outputs = { { kFlowValuePin, value, {} } };
        sw.displayName = "Switch (" + flow.displayName + ")";
        sw.category = "Flow";
        sw.description = "Picks one of its cases by the selector - only that case is computed (shared/NodeSystem/FLOW.md). "
                         "Wire a Choice param into the selector to name the cases after its options.";
        sw.diagramTypes = diagramTypes;
        sw.dynamicPins = true;
        registry.Register(std::move(sw));

        NodeTypeDescriptor route;
        route.typeName = kRoutePrefix + flow.suffix;
        route.domain = Domain::Core;
        route.inputs = { { kFlowSelectorPin, any, std::int64_t { 0 } }, { kFlowCasesPin, integer, std::int64_t { 2 } },
                         { kFlowValuePin, value, flow.type == DataType::Texture ? PinDefaultValue { std::string() } : DefaultValueFor(flow.type) } };
        route.displayName = "Route (" + flow.displayName + ")";
        route.category = "Flow";
        route.description = "Sends its input out on the case the selector picks; the other outputs carry nothing, so "
                            "nothing after them is computed (shared/NodeSystem/FLOW.md).";
        route.diagramTypes = diagramTypes;
        route.dynamicPins = true;
        registry.Register(std::move(route));
    }
}

FlowKind FlowKindOf(const Node& node) {
    const auto& name = node.TypeName();
    if (name.rfind(kSwitchPrefix, 0) == 0) {
        return FlowKind::switchNode;
    }
    if (name.rfind(kRoutePrefix, 0) == 0) {
        return FlowKind::route;
    }
    return FlowKind::none;
}

std::vector<std::string> FlowCaseNames(const Graph& graph, const NodeTypeRegistry& registry, const Node& node) {
    // What drives the selector: an enum names the cases, a toggle makes them Off / On.
    if (const Pin* selector = FindInput(node, kFlowSelectorPin)) {
        for (const auto& wire : graph.Connections()) {
            if (wire.toNode != node.Id() || wire.toPin != selector->id) {
                continue;
            }
            const Node* from = graph.FindNode(wire.fromNode);
            const Pin* fromPin = from != nullptr ? from->FindPin(wire.fromPin) : nullptr;
            if (fromPin == nullptr) {
                break;
            }
            if (const EnumDef* def = PinEnum(registry, *from, *fromPin)) {
                std::vector<std::string> names;
                for (const auto& variant : def->variants) {
                    names.push_back(PinName(variant));
                }
                if (!names.empty()) {
                    return names;
                }
            }
            if (fromPin->type.dataType == DataType::Bool) {
                return { "Off", "On" };
            }
        }
    }
    int count = 2;
    if (const Pin* cases = FindInput(node, kFlowCasesPin)) {
        if (const auto* n = std::get_if<std::int64_t>(&cases->defaultValue)) {
            count = static_cast<int>(std::clamp<std::int64_t>(*n, 1, 64));
        }
    }
    std::vector<std::string> names;
    for (int i = 0; i < count; ++i) {
        names.push_back("case_" + std::to_string(i));
    }
    return names;
}

std::vector<const Pin*> FlowCasePins(const Node& node) {
    const FlowKind kind = FlowKindOf(node);
    std::vector<const Pin*> pins;
    if (kind == FlowKind::switchNode) {
        for (const auto& pin : node.Inputs()) {
            if (!IsFixedInput(kind, pin.name)) {
                pins.push_back(&pin);
            }
        }
    } else if (kind == FlowKind::route) {
        for (const auto& pin : node.Outputs()) {
            pins.push_back(&pin);
        }
    }
    return pins;
}

bool SyncFlowNodeCases(Graph& graph, const NodeTypeRegistry& registry, NodeId id) {
    Node* node = graph.FindNode(id);
    if (node == nullptr) {
        return false;
    }
    const FlowKind kind = FlowKindOf(*node);
    if (kind == FlowKind::none) {
        return false;
    }
    const auto* descriptor = registry.Find(node->TypeName());
    if (descriptor == nullptr) {
        return false;
    }
    const PinTypeDesc caseType = kind == FlowKind::switchNode ? descriptor->outputs.front().type : descriptor->inputs.back().type;
    const auto names = FlowCaseNames(graph, registry, *node);

    // Already right: same names in the same order.
    std::vector<std::string> current;
    for (const Pin* pin : FlowCasePins(*node)) {
        current.push_back(pin->name);
    }
    if (current == names) {
        return false;
    }

    // Remove the case pins whose names are gone, then add the missing ones; matching pins keep their wires.
    std::vector<PinId> remove;
    for (const Pin* pin : FlowCasePins(*node)) {
        if (std::find(names.begin(), names.end(), pin->name) == names.end()) {
            remove.push_back(pin->id);
        }
    }
    for (PinId pin : remove) {
        graph.DisconnectPin(id, pin);
        node->RemovePin(pin);
    }
    std::vector<std::string> kept;
    for (const Pin* pin : FlowCasePins(*node)) {
        kept.push_back(pin->name);
    }
    for (const auto& name : names) {
        if (std::find(kept.begin(), kept.end(), name) != kept.end()) {
            continue;
        }
        if (kind == FlowKind::switchNode) {
            node->AddInput(name, caseType, caseType.dataType == DataType::Texture ? PinDefaultValue { std::string() }
                                                                                 : DefaultValueFor(caseType.dataType));
        } else {
            node->AddOutput(name, caseType, {});
        }
    }
    return true;
}

int FlowCaseIndex(const PinDefaultValue& selector, int count) {
    if (count <= 0) {
        return 0;
    }
    long long index = 0;
    if (const auto* i = std::get_if<std::int64_t>(&selector)) {
        index = *i;
    } else if (const auto* f = std::get_if<float>(&selector)) {
        index = static_cast<long long>(std::floor(*f));
    } else if (const auto* b = std::get_if<bool>(&selector)) {
        index = *b ? 1 : 0;
    }
    return static_cast<int>(std::clamp<long long>(index, 0, count - 1));
}

} // namespace ce::node_system
