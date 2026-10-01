#include "node_system/graph.h"

#include <algorithm>

namespace ce::node_system {

Graph::Graph(std::string name, GraphTarget target) : name_(std::move(name)), target_(target) {}

Node& Graph::AddNode(std::string typeName, Domain domain) {
    NodeId id = nextNodeId_++;
    auto node = std::make_unique<Node>(id, std::move(typeName), domain);
    Node& ref = *node;
    nodes_.emplace(id, std::move(node));
    return ref;
}

Node* Graph::AddNodeWithId(NodeId id, std::string typeName, Domain domain) {
    if (nodes_.find(id) != nodes_.end()) {
        return nullptr;
    }
    auto node = std::make_unique<Node>(id, std::move(typeName), domain);
    Node* ref = node.get();
    nodes_.emplace(id, std::move(node));
    nextNodeId_ = std::max(nextNodeId_, id + 1);
    return ref;
}

bool Graph::RemoveNode(NodeId id) {
    if (nodes_.erase(id) == 0) {
        return false;
    }
    connections_.erase(
        std::remove_if(connections_.begin(), connections_.end(),
                        [id](const Connection& c) { return c.fromNode == id || c.toNode == id; }),
        connections_.end());
    return true;
}

Node* Graph::FindNode(NodeId id) {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : it->second.get();
}

const Node* Graph::FindNode(NodeId id) const {
    auto it = nodes_.find(id);
    return it == nodes_.end() ? nullptr : it->second.get();
}

std::optional<ConnectionId> Graph::Connect(NodeId fromNode, PinId fromPin, NodeId toNode, PinId toPin,
                                            ConnectError* outError) {
    return ConnectInternal(std::nullopt, fromNode, fromPin, toNode, toPin, outError);
}

std::optional<ConnectionId> Graph::ConnectWithId(ConnectionId id, NodeId fromNode, PinId fromPin, NodeId toNode,
                                                  PinId toPin, ConnectError* outError) {
    return ConnectInternal(id, fromNode, fromPin, toNode, toPin, outError);
}

std::optional<ConnectionId> Graph::ConnectInternal(std::optional<ConnectionId> explicitId, NodeId fromNode,
                                                     PinId fromPin, NodeId toNode, PinId toPin,
                                                     ConnectError* outError) {
    auto setError = [&](ConnectError err) {
        if (outError) *outError = err;
        return std::nullopt;
    };

    if (explicitId.has_value()) {
        const bool idInUse =
            std::any_of(connections_.begin(), connections_.end(), [&](const Connection& c) { return c.id == *explicitId; });
        if (idInUse) {
            return std::nullopt; // caller (DeserializeGraph) is expected to have already validated this.
        }
    }

    Node* from = FindNode(fromNode);
    Node* to = FindNode(toNode);
    if (!from || !to) {
        return setError(ConnectError::UnknownNode);
    }

    const Pin* outPin = from->FindPin(fromPin);
    const Pin* inPin = to->FindPin(toPin);
    if (!outPin || !inPin) {
        return setError(ConnectError::UnknownPin);
    }

    if (outPin->isInput || !inPin->isInput) {
        return setError(ConnectError::WrongPinDirection);
    }

    if (!IsConnectionCompatible(outPin->type, inPin->type)) {
        return setError(ConnectError::IncompatibleTypes);
    }

    ConnectionId id = explicitId.value_or(nextConnectionId_++);
    if (explicitId.has_value()) {
        nextConnectionId_ = std::max(nextConnectionId_, id + 1);
    }
    connections_.push_back(Connection{ id, fromNode, fromPin, toNode, toPin });
    return id;
}

void Graph::DisconnectPin(NodeId node, PinId pin) {
    connections_.erase(std::remove_if(connections_.begin(), connections_.end(),
                                      [node, pin](const Connection& c) {
                                          return (c.fromNode == node && c.fromPin == pin) || (c.toNode == node && c.toPin == pin);
                                      }),
                       connections_.end());
}

bool Graph::Disconnect(ConnectionId id) {
    auto it = std::find_if(connections_.begin(), connections_.end(),
                            [id](const Connection& c) { return c.id == id; });
    if (it == connections_.end()) {
        return false;
    }
    connections_.erase(it);
    return true;
}

const EnumDef* Graph::FindEnum(const std::string& name) const {
    for (const auto& def : enums_)
        if (def.name == name)
            return &def;
    return nullptr;
}

EnumDef* Graph::FindEnum(const std::string& name) {
    for (auto& def : enums_)
        if (def.name == name)
            return &def;
    return nullptr;
}

bool Graph::AddEnum(EnumDef def) {
    if (def.name.empty() || def.name.find_first_of(" \t") != std::string::npos || FindEnum(def.name) != nullptr)
        return false;
    def.scope = TypeScope::graph; // a graph's own enums are in its scope
    enums_.push_back(std::move(def));
    return true;
}

bool Graph::RemoveEnum(const std::string& name) {
    const auto it = std::find_if(enums_.begin(), enums_.end(), [&name](const EnumDef& e) { return e.name == name; });
    if (it == enums_.end())
        return false;
    enums_.erase(it);
    return true;
}

const Symbol* Graph::FindSymbol(const std::string& id) const {
    for (const auto& symbol : symbols_)
        if (symbol.id == id)
            return &symbol;
    return nullptr;
}

Symbol* Graph::FindSymbol(const std::string& id) {
    for (auto& symbol : symbols_)
        if (symbol.id == id)
            return &symbol;
    return nullptr;
}

bool Graph::AddSymbol(Symbol symbol) {
    if (symbol.id.empty() || symbol.id.find_first_of(" 	") != std::string::npos || FindSymbol(symbol.id) != nullptr)
        return false;
    symbols_.push_back(std::move(symbol));
    return true;
}

bool Graph::RemoveSymbol(const std::string& id) {
    for (auto it = symbols_.begin(); it != symbols_.end(); ++it)
        if (it->id == id) {
            symbols_.erase(it);
            return true;
        }
    return false;
}

} // namespace ce::node_system
