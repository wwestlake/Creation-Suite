#pragma once

#include <string>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

// Get nodes for graph symbols (symbols.h) - the one way a node graph reads its params, constants and variables.
// One node type per data type ("core.symbol.get.float", ".int", ".bool", ".color", ".vec3", ".string"), each with
// a "symbol" input holding the symbol's id and a "value" output of that type. How the value is produced is the
// host's job (an evaluator, or a code generator): SymbolForGetNode finds the symbol, and a param's value can be
// overridden from outside. See shared/NodeSystem/SYMBOLS.md.
namespace ce::node_system {

inline constexpr const char* kSymbolIdPin = "symbol";
inline constexpr const char* kSymbolValuePin = "value";

void RegisterSymbolGetNodes(NodeTypeRegistry& registry);

// The Get node type for a data type, or "" if symbols of that type have no Get node.
std::string SymbolGetNodeType(DataType type);
bool IsSymbolGetNode(const std::string& typeName);

// The symbol a Get node reads, or null if its id is empty or no longer in the graph.
const Symbol* SymbolForGetNode(const Graph& graph, const Node& node);

// Adds a Get node already bound to `symbol` (for example when a symbol is dragged onto the graph).
Node* AddSymbolGetNode(Graph& graph, const NodeTypeRegistry& registry, const Symbol& symbol, std::string* errorOut = nullptr);

} // namespace ce::node_system
