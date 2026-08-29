#include "node_system/graph_analysis.h"

#include <algorithm>
#include <functional>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace ce::node_system {

namespace {

// Builds an adjacency list over `graph`'s connections, keeping only
// ones whose source pin matches `kindFilter` -- shared by both
// DetectExecCycle (Exec) and TopologicalDataOrder (Data).
std::unordered_map<NodeId, std::vector<NodeId>> BuildAdjacency(const Graph& graph, PinKind kindFilter) {
    std::unordered_map<NodeId, std::vector<NodeId>> adjacency;
    for (const Connection& conn : graph.Connections()) {
        const Node* fromNode = graph.FindNode(conn.fromNode);
        if (fromNode == nullptr) {
            continue;
        }
        const Pin* fromPin = fromNode->FindPin(conn.fromPin);
        if (fromPin == nullptr || fromPin->type.kind != kindFilter) {
            continue;
        }
        adjacency[conn.fromNode].push_back(conn.toNode);
    }
    return adjacency;
}

} // namespace

std::optional<std::vector<NodeId>> DetectExecCycle(const Graph& graph) {
    const auto adjacency = BuildAdjacency(graph, PinKind::Exec);

    enum class VisitState { Unvisited, InProgress, Done };
    std::unordered_map<NodeId, VisitState> state;
    std::vector<NodeId> pathStack;
    std::optional<std::vector<NodeId>> cycle;

    std::function<bool(NodeId)> visit = [&](NodeId nodeId) -> bool {
        state[nodeId] = VisitState::InProgress;
        pathStack.push_back(nodeId);

        const auto it = adjacency.find(nodeId);
        if (it != adjacency.end()) {
            for (NodeId next : it->second) {
                const auto found = state.find(next);
                if (found == state.end() || found->second == VisitState::Unvisited) {
                    if (visit(next)) {
                        return true;
                    }
                } else if (found->second == VisitState::InProgress) {
                    const auto startOfCycle = std::find(pathStack.begin(), pathStack.end(), next);
                    std::vector<NodeId> path(startOfCycle, pathStack.end());
                    path.push_back(next);
                    cycle = std::move(path);
                    return true;
                }
            }
        }

        pathStack.pop_back();
        state[nodeId] = VisitState::Done;
        return false;
    };

    for (const auto& [id, node] : graph.Nodes()) {
        if (state.find(id) == state.end()) {
            if (visit(id)) {
                return cycle;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::vector<NodeId>> TopologicalDataOrder(const Graph& graph) {
    const auto adjacency = BuildAdjacency(graph, PinKind::Data);

    std::unordered_map<NodeId, int> inDegree;
    for (const auto& [id, node] : graph.Nodes()) {
        inDegree[id] = 0;
    }
    for (const auto& [from, targets] : adjacency) {
        for (NodeId to : targets) {
            inDegree[to]++;
        }
    }

    // Sorted seed so the result is deterministic across runs (map/set
    // iteration order isn't guaranteed) -- matters for
    // celc --selftest-graph's structural-equality assertions and for
    // .celg round-trip stability if this were ever serialized.
    std::vector<NodeId> ready;
    for (const auto& [id, degree] : inDegree) {
        if (degree == 0) {
            ready.push_back(id);
        }
    }
    std::sort(ready.begin(), ready.end());
    std::priority_queue<NodeId, std::vector<NodeId>, std::greater<>> queue(ready.begin(), ready.end());

    std::vector<NodeId> order;
    while (!queue.empty()) {
        const NodeId current = queue.top();
        queue.pop();
        order.push_back(current);

        const auto it = adjacency.find(current);
        if (it == adjacency.end()) {
            continue;
        }
        for (NodeId next : it->second) {
            if (--inDegree[next] == 0) {
                queue.push(next);
            }
        }
    }

    if (order.size() != graph.Nodes().size()) {
        return std::nullopt; // a data cycle left some node(s) permanently in-degree > 0.
    }
    return order;
}

ValidationResult ValidateGraph(const Graph& graph, const NodeTypeRegistry* registry) {
    ValidationResult result;

    const auto execCycle = DetectExecCycle(graph);
    bool legalLoopCycle = false;
    if (execCycle.has_value() && registry != nullptr) {
        legalLoopCycle = std::any_of(execCycle->begin(), execCycle->end(), [&](NodeId id) {
            const auto* node = graph.FindNode(id);
            const auto* descriptor = node != nullptr ? registry->Find(node->TypeName()) : nullptr;
            return descriptor != nullptr && (descriptor->controlFlow == ControlFlowKind::For
                                           || descriptor->controlFlow == ControlFlowKind::While);
        });
    }

    if (execCycle.has_value() && ! legalLoopCycle) {
        result.ok = false;
        std::string msg = "exec cycle detected through node(s): ";
        for (std::size_t i = 0; i < execCycle->size(); ++i) {
            if (i > 0) msg += " -> ";
            msg += std::to_string((*execCycle)[i]);
        }
        result.errors.push_back(msg);
    }

    if (!TopologicalDataOrder(graph).has_value()) {
        result.ok = false;
        result.errors.push_back("data dependency cycle detected");
    }

    if (registry != nullptr) {
        std::vector<std::string> registryErrors;
        if (!ValidateAgainstRegistry(graph, *registry, &registryErrors)) {
            result.ok = false;
            for (auto& err : registryErrors) {
                result.errors.push_back(std::move(err));
            }
        }
        const auto controlFlow = ValidateControlFlow(graph, *registry);
        if (! controlFlow.ok) {
            result.ok = false;
            result.errors.insert(result.errors.end(), controlFlow.errors.begin(), controlFlow.errors.end());
        }
    }

    return result;
}

ValidationResult ValidateControlFlow(const Graph& graph, const NodeTypeRegistry& registry)
{
    ValidationResult result;
    for (const auto& [id, node] : graph.Nodes()) {
        const auto* descriptor = registry.Find(node->TypeName());
        if (descriptor == nullptr)
            continue;

        const auto kind = descriptor->controlFlow;
        if (kind == ControlFlowKind::For) {
            const Pin* step = nullptr;
            for (const auto& pin : node->Inputs()) {
                if (pin.name == "step") {
                    step = &pin;
                    break;
                }
            }
            if (step != nullptr && std::holds_alternative<std::int64_t>(step->defaultValue)
                && std::get<std::int64_t>(step->defaultValue) == 0)
                result.errors.push_back("node " + std::to_string(id) + " (For) cannot have a zero step");
        }

        if (kind == ControlFlowKind::Break || kind == ControlFlowKind::Continue) {
            bool hasLoop = false;
            for (const auto& [otherId, other] : graph.Nodes()) {
                const auto* otherDescriptor = registry.Find(other->TypeName());
                if (otherDescriptor != nullptr && (otherDescriptor->controlFlow == ControlFlowKind::For
                    || otherDescriptor->controlFlow == ControlFlowKind::While) && otherId != id) {
                    hasLoop = true;
                    break;
                }
            }
            if (! hasLoop)
                result.errors.push_back("node " + std::to_string(id) + " ("
                    + (kind == ControlFlowKind::Break ? "Break" : "Continue")
                    + ") must be inside a loop");
        }
    }

    result.ok = result.errors.empty();
    return result;
}

} // namespace ce::node_system
