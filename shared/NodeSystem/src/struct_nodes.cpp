#include "node_system/struct_nodes.h"

#include <algorithm>
#include <sstream>

namespace ce::node_system {

namespace {
PinDefaultValue MemberDefault(const StructMember& member) {
    if (member.type.dataType == DataType::Texture) {
        return std::string(); // an image member, like any image input, may name a project image
    }
    if (member.type.dataType == DataType::Struct || member.type.dataType == DataType::Drawing || member.type.dataType == DataType::Brush) {
        return {};
    }
    return std::holds_alternative<std::monostate>(member.defaultValue) ? DefaultValueFor(member.type.dataType) : member.defaultValue;
}

// Wires to or from a pin that no longer fit its type go.
void DropMisfitWires(Graph& graph, NodeId nodeId, PinId pinId) {
    const Node* node = graph.FindNode(nodeId);
    const Pin* pin = node != nullptr ? node->FindPin(pinId) : nullptr;
    if (pin == nullptr) {
        return;
    }
    std::vector<ConnectionId> misfits;
    for (const auto& wire : graph.Connections()) {
        const bool from = wire.fromNode == nodeId && wire.fromPin == pinId;
        const bool to = wire.toNode == nodeId && wire.toPin == pinId;
        if (!from && !to) {
            continue;
        }
        const Node* other = graph.FindNode(from ? wire.toNode : wire.fromNode);
        const Pin* otherPin = other != nullptr ? other->FindPin(from ? wire.toPin : wire.fromPin) : nullptr;
        if (otherPin == nullptr || !(from ? IsConnectionCompatible(pin->type, otherPin->type) : IsConnectionCompatible(otherPin->type, pin->type))) {
            misfits.push_back(wire.id);
        }
    }
    for (auto id : misfits) {
        graph.Disconnect(id);
    }
}

std::vector<std::string> SplitNames(const std::string& text) {
    std::vector<std::string> names;
    std::stringstream in(text);
    std::string item;
    while (std::getline(in, item, ',')) {
        const auto first = item.find_first_not_of(' ');
        const auto last = item.find_last_not_of(' ');
        if (first != std::string::npos) {
            names.push_back(item.substr(first, last - first + 1));
        }
    }
    return names;
}

// The node's own pins, not members: "type", the struct pin ("value", in or out), and the member choices. Member names
// cannot be these (the Struct Editor refuses them - StructMemberNameReserved).
bool IsFixedPin(StructNodeKind kind, const Pin& pin) {
    if (pin.name == kStructTypePin || pin.name == kStructValuePin) return true;
    if (pin.name == kStructMembersPin && kind == StructNodeKind::setMembers && pin.isInput) return true;
    if (pin.name == kStructMemberPin && kind == StructNodeKind::getMember && pin.isInput) return true;
    return false;
}
} // namespace

std::string StructMemberPinName(const std::string& memberName) {
    std::string name = memberName;
    std::replace(name.begin(), name.end(), ' ', '_'); // pin names have no spaces (frgraph)
    return name;
}

bool StructMemberNameReserved(const std::string& memberName) {
    const auto name = StructMemberPinName(memberName);
    return name.empty() || name == kStructTypePin || name == kStructValuePin || name == kStructMembersPin || name == kStructMemberPin;
}

void RegisterStructNodes(NodeTypeRegistry& registry, std::vector<std::string> diagramTypes) {
    const PinTypeDesc text { PinKind::Data, DataType::String };
    const PinTypeDesc structType { PinKind::Data, DataType::Struct };
    auto add = [&](const char* typeName, const char* displayName, const char* description, std::vector<PinSignature> inputs,
                   std::vector<PinSignature> outputs) {
        NodeTypeDescriptor d;
        d.typeName = typeName;
        d.domain = Domain::Core;
        d.inputs = std::move(inputs);
        d.outputs = std::move(outputs);
        d.displayName = displayName;
        d.category = "Structs";
        d.description = description;
        d.diagramTypes = diagramTypes;
        d.dynamicPins = true;
        registry.Register(std::move(d));
    };
    add(kMakeStructType, "Make Struct", "Builds a struct from its members. Choose the struct in Properties - structs are made in the Types panel.",
        { { kStructTypePin, text, std::string() } }, { { kStructValuePin, structType, {} } });
    add(kBreakStructType, "Break Struct", "Takes a struct apart into all its members.",
        { { kStructTypePin, text, std::string() }, { kStructValuePin, structType, {} } }, {});
    add(kSetMembersType, "Set Members", "The struct with the members ticked in Properties changed, the rest passed through.",
        { { kStructTypePin, text, std::string() }, { kStructValuePin, structType, {} }, { kStructMembersPin, text, std::string() } },
        { { kStructValuePin, structType, {} } });
    add(kGetMemberType, "Get Member", "One member of a struct, chosen in Properties.",
        { { kStructTypePin, text, std::string() }, { kStructValuePin, structType, {} }, { kStructMemberPin, text, std::string() } }, {});
}

StructNodeKind StructNodeKindOf(const Node& node) {
    const auto& t = node.TypeName();
    if (t == kMakeStructType) return StructNodeKind::make;
    if (t == kBreakStructType) return StructNodeKind::breakApart;
    if (t == kSetMembersType) return StructNodeKind::setMembers;
    if (t == kGetMemberType) return StructNodeKind::getMember;
    return StructNodeKind::none;
}

std::string StructNodeText(const Node& node, const char* pinName) {
    for (const auto& pin : node.Inputs()) {
        if (pin.name == pinName) {
            if (const auto* text = std::get_if<std::string>(&pin.defaultValue)) {
                return *text;
            }
        }
    }
    return {};
}

std::string StructNodeType(const Node& node) {
    return StructNodeText(node, kStructTypePin);
}

const StructDef* FindStructFor(const Graph& graph, const NodeTypeRegistry& registry, const std::string& name) {
    if (name.empty()) {
        return nullptr;
    }
    if (const auto* own = graph.FindStruct(name)) {
        return own;
    }
    return registry.FindStruct(name);
}

bool SyncStructNodePins(Graph& graph, const NodeTypeRegistry& registry, NodeId id) {
    Node* node = graph.FindNode(id);
    if (node == nullptr) {
        return false;
    }
    const StructNodeKind kind = StructNodeKindOf(*node);
    if (kind == StructNodeKind::none) {
        return false;
    }
    const std::string structName = StructNodeType(*node);
    const StructDef* def = FindStructFor(graph, registry, structName);
    const std::string carried = def != nullptr ? structName : std::string();
    bool changed = false;

    // The struct pins carry this struct.
    std::vector<PinId> structPins;
    for (const auto& pin : node->Inputs())
        if (pin.name == kStructValuePin && pin.type.dataType == DataType::Struct) structPins.push_back(pin.id);
    for (const auto& pin : node->Outputs())
        if (pin.name == kStructValuePin && pin.type.dataType == DataType::Struct) structPins.push_back(pin.id);
    for (PinId pinId : structPins) {
        Pin* pin = node->FindPin(pinId);
        if (pin->type.structType != carried) {
            pin->type.structType = carried;
            DropMisfitWires(graph, id, pinId);
            changed = true;
        }
    }

    // The members this node shows: all (Make, Break), the ticked ones (Set Members), the chosen one (Get Member).
    std::vector<const StructMember*> members;
    if (def != nullptr) {
        const auto ticked = SplitNames(StructNodeText(*node, kStructMembersPin));
        const auto chosen = StructNodeText(*node, kStructMemberPin);
        for (const auto& m : def->members) {
            if (kind == StructNodeKind::setMembers && std::find(ticked.begin(), ticked.end(), m.name) == ticked.end()) continue;
            if (kind == StructNodeKind::getMember && m.name != chosen) continue;
            members.push_back(&m);
        }
    }
    const bool asInputs = kind == StructNodeKind::make || kind == StructNodeKind::setMembers;

    std::vector<PinId> current;
    for (const auto& pin : asInputs ? node->Inputs() : node->Outputs())
        if (!IsFixedPin(kind, pin)) current.push_back(pin.id);
    std::vector<std::string> wanted;
    for (const auto* m : members) {
        wanted.push_back(StructMemberPinName(m->name));
    }

    // Members match by name, so reordering a struct keeps each wire with its member. Of the rest, as many pins as
    // there are new names are renamed (a member renamed: the wire stays); spare pins go, missing members get pins.
    std::vector<PinId> unmatched;
    std::vector<std::string> currentNames;
    for (PinId pinId : current) {
        const auto& name = node->FindPin(pinId)->name;
        currentNames.push_back(name);
        if (std::find(wanted.begin(), wanted.end(), name) == wanted.end()) {
            unmatched.push_back(pinId);
        }
    }
    std::vector<const StructMember*> missing;
    for (const auto* m : members) {
        if (std::find(currentNames.begin(), currentNames.end(), StructMemberPinName(m->name)) == currentNames.end()) {
            missing.push_back(m);
        }
    }
    size_t renamed = 0;
    for (; renamed < unmatched.size() && renamed < missing.size(); ++renamed) {
        node->FindPin(unmatched[renamed])->name = StructMemberPinName(missing[renamed]->name);
        changed = true;
    }
    for (size_t i = renamed; i < unmatched.size(); ++i) {
        graph.DisconnectPin(id, unmatched[i]);
        node->RemovePin(unmatched[i]);
        changed = true;
    }
    for (size_t i = renamed; i < missing.size(); ++i) {
        if (asInputs) {
            node->AddInput(StructMemberPinName(missing[i]->name), missing[i]->type, MemberDefault(*missing[i]));
        } else {
            node->AddOutput(StructMemberPinName(missing[i]->name), missing[i]->type, {});
        }
        changed = true;
    }

    // Each member pin takes its member's type (a retyped member: wires that no longer fit go), in the struct's order.
    std::vector<PinId> order;
    for (const auto* m : members) {
        for (const auto& pin : asInputs ? node->Inputs() : node->Outputs()) {
            if (IsFixedPin(kind, pin) || pin.name != StructMemberPinName(m->name)) {
                continue;
            }
            if (!(pin.type == m->type)) {
                Pin* p = node->FindPin(pin.id);
                p->type = m->type;
                if (asInputs) {
                    p->defaultValue = MemberDefault(*m);
                }
                DropMisfitWires(graph, id, pin.id);
                changed = true;
            }
            order.push_back(pin.id);
        }
    }
    std::vector<PinId> before;
    for (const auto& pin : asInputs ? node->Inputs() : node->Outputs())
        if (!IsFixedPin(kind, pin)) before.push_back(pin.id);
    if (before != order) {
        node->ArrangePins(asInputs, order);
        changed = true;
    }
    return changed;
}

} // namespace ce::node_system
