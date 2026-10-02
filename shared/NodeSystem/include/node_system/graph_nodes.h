#pragma once

#include <string>
#include <vector>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

// Graphs as nodes (GRAPH_TYPES.md, phase 2). A graph has an interface - what it takes in and gives out - so another
// graph can use it as a node. Research's schematic calls the used graph a subgraph / node function and the node a
// call_function node; here it is the Graph node, "core.graph".
//
//   Inputs  - the graph's params (symbols.h), each a port named by its id, typed by the param (Choices keep their
//             enum), the param's value as the default; then its graph-input nodes.
//   Outputs - its graph-output nodes.
//
// A graph-input or graph-output node is an ordinary node type whose descriptor sets graphPort. It names its port with
// a text input pin called "name". A graph-input node's port takes the type of its first output pin; a graph-output
// node's, the type of its first input pin other than "name".
//
// The Graph node has one fixed input, "graph" (which graph it uses - a project path, resolved by the app), and then
// one pin per port. Its pins follow the used graph: SyncGraphNodePins keeps the pins (their wires and typed-in values)
// that still match a port, removes the rest with their wires, and adds new ones. How it is evaluated or compiled is
// the app's job, as for every node.
namespace ce::node_system {

inline constexpr const char* kGraphNodeType = "core.graph";
inline constexpr const char* kGraphPathPin = "graph";
inline constexpr const char* kGraphPortNamePin = "name";

struct InterfacePort {
    std::string name;      // the pin name on a Graph node
    std::string label;     // shown to people (a param's display name)
    PinTypeDesc type;
    PinDefaultValue defaultValue;
    bool fromParam = false;
};

struct GraphInterface {
    std::vector<InterfacePort> inputs;
    std::vector<InterfacePort> outputs;
};

GraphInterface InterfaceOf(const Graph& graph, const NodeTypeRegistry& registry);

// The name a graph-input / graph-output node gives its port ("name" pin; "input" / "output" if empty).
std::string GraphPortName(const Node& node, GraphPort role);

// Registers the Graph node ("core.graph", category "Graphs") for the given graph types (empty: every type).
void RegisterGraphNode(NodeTypeRegistry& registry, std::vector<std::string> diagramTypes = {});

bool IsGraphNode(const Node& node);
// The graph a Graph node uses (its "graph" pin), or "".
std::string GraphNodePath(const Node& node);

// Rebuilds a Graph node's pins after the interface of the graph it uses (see above). False if the node is missing.
bool SyncGraphNodePins(Graph& host, NodeId graphNode, const GraphInterface& graphInterface);

} // namespace ce::node_system
