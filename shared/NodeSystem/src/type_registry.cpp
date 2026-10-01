#include "node_system/type_registry.h"

#include <algorithm>
#include <sstream>

namespace ce::node_system {

void NodeTypeRegistry::Register(NodeTypeDescriptor descriptor) {
    const std::string key = descriptor.typeName;
    types_[key] = std::move(descriptor);
}

void NodeTypeRegistry::Unregister(const std::string& typeName) {
    types_.erase(typeName);
}

const NodeTypeDescriptor* NodeTypeRegistry::Find(const std::string& typeName) const {
    const auto it = types_.find(typeName);
    return it == types_.end() ? nullptr : &it->second;
}

void NodeTypeRegistry::RegisterEnum(EnumDef def) {
    for (auto& existing : enums_) {
        if (existing.name == def.name) {
            existing = std::move(def);
            return;
        }
    }
    enums_.push_back(std::move(def));
}

const EnumDef* NodeTypeRegistry::FindEnum(const std::string& name) const {
    for (const auto& def : enums_) {
        if (def.name == name) {
            return &def;
        }
    }
    return nullptr;
}

bool AllowedInDiagram(const NodeTypeDescriptor& descriptor, const std::string& diagramType) {
    return diagramType.empty() || descriptor.diagramTypes.empty()
        || std::find(descriptor.diagramTypes.begin(), descriptor.diagramTypes.end(), diagramType) != descriptor.diagramTypes.end();
}

void NodeTypeRegistry::RegisterDiagramType(DiagramTypeDef def) {
    for (auto& existing : diagramTypes_) {
        if (existing.id == def.id) {
            existing = std::move(def);
            return;
        }
    }
    diagramTypes_.push_back(std::move(def));
}

const DiagramTypeDef* NodeTypeRegistry::FindDiagramType(const std::string& id) const {
    for (const auto& def : diagramTypes_) {
        if (def.id == id) {
            return &def;
        }
    }
    return nullptr;
}

std::vector<NodeId> NodesNotAllowedIn(const Graph& graph, const NodeTypeRegistry& registry, const std::string& diagramType) {
    std::vector<NodeId> blocked;
    for (const auto& [id, node] : graph.Nodes()) {
        const auto* descriptor = registry.Find(node->TypeName());
        if (descriptor == nullptr || !AllowedInDiagram(*descriptor, diagramType)) {
            blocked.push_back(id);
        }
    }
    std::sort(blocked.begin(), blocked.end());
    return blocked;
}

const EnumDef* PinEnum(const NodeTypeRegistry& registry, const Node& node, const Pin& pin) {
    if (!pin.type.enumType.empty()) {
        return registry.FindEnum(pin.type.enumType);
    }
    if (const auto* descriptor = registry.Find(node.TypeName())) {
        for (const auto& sig : pin.isInput ? descriptor->inputs : descriptor->outputs) {
            if (sig.name == pin.name) {
                return sig.type.enumType.empty() ? nullptr : registry.FindEnum(sig.type.enumType);
            }
        }
    }
    return nullptr;
}

Node* AddRegisteredNode(Graph& graph, const NodeTypeRegistry& registry, const std::string& typeName,
                         std::string* errorOut) {
    const NodeTypeDescriptor* descriptor = registry.Find(typeName);
    if (descriptor == nullptr) {
        if (errorOut) {
            *errorOut = "unregistered node type '" + typeName + "'";
        }
        return nullptr;
    }

    Node& node = graph.AddNode(typeName, descriptor->domain);
    for (const PinSignature& sig : descriptor->inputs) {
        node.AddInput(sig.name, sig.type, sig.defaultValue);
    }
    for (const PinSignature& sig : descriptor->outputs) {
        node.AddOutput(sig.name, sig.type, sig.defaultValue);
    }
    return &node;
}

namespace {

// A pin matches its signature's type; a pin may carry an enum its signature leaves open (a Get node bound to a
// Choice symbol), but never a different one.
bool PinTypeMatchesSignature(const PinTypeDesc& pin, const PinTypeDesc& signature) {
    return pin.kind == signature.kind && pin.dataType == signature.dataType && pin.monad == signature.monad
        && (signature.enumType.empty() || signature.enumType == pin.enumType);
}

bool PinsMatchSignature(const std::vector<Pin>& pins, const std::vector<PinSignature>& signatures,
                         const std::string& nodeTypeName, const char* direction, std::vector<std::string>* errorsOut,
                         bool dynamicPins = false) {
    bool ok = true;
    if (dynamicPins ? pins.size() < signatures.size() : pins.size() != signatures.size()) {
        ok = false;
        if (errorsOut) {
            std::ostringstream msg;
            msg << "node type '" << nodeTypeName << "': expected " << signatures.size() << " " << direction
                << " pin(s), found " << pins.size();
            errorsOut->push_back(msg.str());
        }
    }
    const std::size_t count = std::min(pins.size(), signatures.size());
    for (std::size_t i = 0; i < count; ++i) {
        // Name and type only -- NOT defaultValue, which a node instance
        // is free to override (see PinSignature/ValidateAgainstRegistry's
        // own comments on why).
        if (pins[i].name != signatures[i].name || !PinTypeMatchesSignature(pins[i].type, signatures[i].type)) {
            ok = false;
            if (errorsOut) {
                std::ostringstream msg;
                msg << "node type '" << nodeTypeName << "': " << direction << " pin " << i << " ('" << pins[i].name
                    << "') doesn't match its registered signature ('" << signatures[i].name << "')";
                errorsOut->push_back(msg.str());
            }
        }
    }
    return ok;
}

} // namespace

bool ValidateAgainstRegistry(const Graph& graph, const NodeTypeRegistry& registry,
                              std::vector<std::string>* errorsOut) {
    bool ok = true;
    for (const auto& [id, node] : graph.Nodes()) {
        const NodeTypeDescriptor* descriptor = registry.Find(node->TypeName());
        if (descriptor == nullptr) {
            ok = false;
            if (errorsOut) {
                errorsOut->push_back("node " + std::to_string(id) + ": unregistered type '" + node->TypeName() + "'");
            }
            continue;
        }
        ok = PinsMatchSignature(node->Inputs(), descriptor->inputs, node->TypeName(), "input", errorsOut, descriptor->dynamicPins) && ok;
        ok = PinsMatchSignature(node->Outputs(), descriptor->outputs, node->TypeName(), "output", errorsOut, descriptor->dynamicPins) && ok;
    }
    return ok;
}

DataType ResolveEffectivePinType(const NodeTypeDescriptor& type, const Node& node, const Pin& pin) {
    if (type.genericParams.empty()) return pin.type.dataType;
    const std::vector<PinSignature>& signatures = pin.isInput ? type.inputs : type.outputs;
    const auto sigIt = std::find_if(signatures.begin(), signatures.end(),
                                     [&](const PinSignature& sig) { return sig.name == pin.name; });
    if (sigIt == signatures.end() || sigIt->genericParam.empty()) return pin.type.dataType;
    const auto& bindings = node.GenericBindings();
    const auto bindingIt = bindings.find(sigIt->genericParam);
    return bindingIt == bindings.end() ? DataType::Any : bindingIt->second;
}

} // namespace ce::node_system
