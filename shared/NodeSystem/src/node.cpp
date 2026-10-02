#include "node_system/node.h"

#include <algorithm>

namespace ce::node_system {

Node::Node(NodeId id, std::string typeName, Domain domain)
    : id_(id), typeName_(std::move(typeName)), domain_(domain) {}

PinId Node::AddInput(const std::string& name, PinTypeDesc type, PinDefaultValue defaultValue) {
    PinId id = nextPinId_++;
    inputs_.push_back(Pin{ id, name, type, /*isInput=*/true, std::move(defaultValue) });
    return id;
}

PinId Node::AddOutput(const std::string& name, PinTypeDesc type, PinDefaultValue defaultValue) {
    PinId id = nextPinId_++;
    outputs_.push_back(Pin{ id, name, type, /*isInput=*/false, std::move(defaultValue) });
    return id;
}

PinId Node::AddInputWithId(PinId id, const std::string& name, PinTypeDesc type, PinDefaultValue defaultValue) {
    inputs_.push_back(Pin{ id, name, type, /*isInput=*/true, std::move(defaultValue) });
    nextPinId_ = std::max(nextPinId_, id + 1);
    return id;
}

PinId Node::AddOutputWithId(PinId id, const std::string& name, PinTypeDesc type, PinDefaultValue defaultValue) {
    outputs_.push_back(Pin{ id, name, type, /*isInput=*/false, std::move(defaultValue) });
    nextPinId_ = std::max(nextPinId_, id + 1);
    return id;
}

const Pin* Node::FindPin(PinId pinId) const {
    for (const auto& pin : inputs_) {
        if (pin.id == pinId) return &pin;
    }
    for (const auto& pin : outputs_) {
        if (pin.id == pinId) return &pin;
    }
    return nullptr;
}

Pin* Node::FindPin(PinId pinId) {
    for (auto& pin : inputs_) {
        if (pin.id == pinId) return &pin;
    }
    for (auto& pin : outputs_) {
        if (pin.id == pinId) return &pin;
    }
    return nullptr;
}

bool Node::RemovePin(PinId pinId) {
    for (auto* pins : { &inputs_, &outputs_ }) {
        auto it = std::find_if(pins->begin(), pins->end(), [pinId](const Pin& p) { return p.id == pinId; });
        if (it != pins->end()) {
            pins->erase(it);
            return true;
        }
    }
    return false;
}

void Node::ArrangePins(bool inputs, const std::vector<PinId>& trailing) {
    auto& pins = inputs ? inputs_ : outputs_;
    std::vector<Pin> arranged;
    for (const auto& pin : pins)
        if (std::find(trailing.begin(), trailing.end(), pin.id) == trailing.end())
            arranged.push_back(pin);
    for (PinId id : trailing)
        for (const auto& pin : pins)
            if (pin.id == id)
                arranged.push_back(pin);
    pins = std::move(arranged);
}

} // namespace ce::node_system
