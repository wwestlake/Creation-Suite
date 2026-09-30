#pragma once

#include <optional>
#include <string>
#include <vector>

#include "node_system/pin.h"

// Graph symbols: the named values that belong to a graph itself - its params, constants and variables. This is the
// suite standard every node-based system follows (docs: shared/NodeSystem/SYMBOLS.md), and it matches the research
// node language's schematic (FrustLang projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md: "params",
// "constants", "variables", with "accessibility"), so a graph lifts into that system unchanged.
//
//   Param     - an input to the graph: its value comes from outside (an Automation, the LLM, a parent graph) and
//               `value` is the default used when nothing sets it.
//   Constant  - a named fixed value, used in several places.
//   Variable  - named state the graph can read and change while it runs (behaviour / execution graphs). A pure
//               data-flow graph reads it like a constant.
//
// Nodes use a symbol through a Get node (symbol_nodes.h) whose "symbol" pin holds the symbol's id; ids are stable
// across renames, so renaming a symbol never breaks the graph.
namespace ce::node_system {

enum class SymbolKind {
    Param,
    Constant,
    Variable,
};

struct Symbol {
    std::string id;                     // stable identifier, referenced by Get nodes; no spaces
    std::string name;                   // display name, may contain spaces
    SymbolKind kind = SymbolKind::Param;
    DataType type = DataType::Float;
    PinDefaultValue value;              // the default (param / variable) or the value (constant)
    // Who can see or use it (schema v2): private, graph, module, project, public, agent, readonly, hidden.
    std::string accessibility = "graph";
    bool persistent = false;            // variables: kept between runs
    std::string description;

    bool operator==(const Symbol&) const = default;
};

std::string SymbolKindToString(SymbolKind kind);           // "param", "constant", "variable"
std::optional<SymbolKind> SymbolKindFromString(const std::string& text);

// The accessibility values schema v2 defines.
const std::vector<std::string>& SymbolAccessibilities();

// A default value of the right shape for a data type (0, false, black, empty text...).
PinDefaultValue DefaultValueFor(DataType type);

// A new id, unique in `existing`, made from a display name ("Tile Scale" -> "tile_scale", then "tile_scale_2"...).
std::string MakeSymbolId(const std::string& name, const std::vector<Symbol>& existing);

} // namespace ce::node_system
