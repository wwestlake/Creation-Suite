#pragma once

#include <string>
#include <vector>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

// Decisions in a data-flow graph (owner, 2026-10-01; shared/NodeSystem/FLOW.md). A decision chooses which data flows,
// not which code runs - like an F# `if ... then f x else g x` or `match`: each branch is a function call and only the
// chosen one is evaluated.
//
//   Switch (many in, one out) - the `match`: a selector picks one of its case inputs; only that one is computed.
//   Route  (one in, many out) - the split: the input goes out on the case output the selector picks; the other
//                               outputs carry nothing, so nothing downstream of them is computed. Where the branches
//                               meet again, a Switch on the same selector picks the live one.
//
// One node type per kind of value ("core.switch.image", "core.route.number"...), so wires stay type-safe. The
// selector takes any value: a number picks case floor(n), an integer case n, a toggle Off / On; out of range is
// clamped. The cases are named after what drives the selector: an enum's variants (a Choice param), Off / On for a
// toggle, otherwise case_0, case_1 ... for the node's "cases" count. SyncFlowNodeCases keeps the case pins in line.
// Evaluating or compiling them is the app's job, as for every node.
namespace ce::node_system {

inline constexpr const char* kSwitchPrefix = "core.switch.";
inline constexpr const char* kRoutePrefix = "core.route.";
inline constexpr const char* kFlowSelectorPin = "selector";
inline constexpr const char* kFlowCasesPin = "cases";
inline constexpr const char* kFlowValuePin = "value"; // Switch's output, Route's input

// A kind of value flow nodes are made for: its data type, the type-name suffix and the name people see.
struct FlowType {
    DataType type;
    std::string suffix;      // "image"
    std::string displayName; // "Image"
};

// The usual suffix and name for a data type (Texture -> image / Image, Float -> number / Number ...).
FlowType StandardFlowType(DataType type);

// Registers Switch and Route for each type, belonging in the given graph types (empty: every type).
void RegisterFlowNodes(NodeTypeRegistry& registry, const std::vector<FlowType>& types, std::vector<std::string> diagramTypes = {});

enum class FlowKind { none, switchNode, route };
FlowKind FlowKindOf(const Node& node);

// The case names the node should have now (see above).
std::vector<std::string> FlowCaseNames(const Graph& graph, const NodeTypeRegistry& registry, const Node& node);

// The case pins - Switch's inputs after selector and cases, Route's outputs - brought in line with FlowCaseNames:
// pins whose name still matches keep their wires and values; the rest go with their wires. True if anything changed.
bool SyncFlowNodeCases(Graph& graph, const NodeTypeRegistry& registry, NodeId node);

// Which of `count` cases a selector value picks.
int FlowCaseIndex(const PinDefaultValue& selector, int count);

// A flow node's case pins in order (Switch: inputs; Route: outputs).
std::vector<const Pin*> FlowCasePins(const Node& node);

} // namespace ce::node_system
