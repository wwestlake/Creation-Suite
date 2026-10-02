#pragma once

#include <string>
#include <vector>

#include "node_system/graph.h"
#include "node_system/type_registry.h"

// Nodes that use structs (TYPES.md) - they never create one; structs are made in the Struct Editor. Modelled on
// UE4's struct nodes, all pure data flow:
//
//   Make Struct   members in, the struct out.
//   Break Struct  the struct in, every member out.
//   Set Members   the struct in, plus only the members ticked in Properties; the struct out with those changed and
//                 the rest passed through.
//   Get Member    the struct in, one chosen member out.
//
// A struct param (Variables) read with its Get node gives the whole struct, to send along as one wire or take apart
// with these. Every one has a fixed "type" input naming the struct (chosen in Properties); Set Members also has
// "members" (the ticked names, comma separated) and Get Member "member". SyncStructNodePins keeps the member pins in
// line with the struct: renamed or retyped members keep their pins in place (and wires, where the type still fits).
namespace ce::node_system {

inline constexpr const char* kMakeStructType = "core.struct.make";
inline constexpr const char* kBreakStructType = "core.struct.break";
inline constexpr const char* kSetMembersType = "core.struct.set";
inline constexpr const char* kGetMemberType = "core.struct.get";
inline constexpr const char* kStructTypePin = "type";       // the struct's name
inline constexpr const char* kStructValuePin = "value";     // the struct going in or out
inline constexpr const char* kStructMembersPin = "members"; // Set Members: which members, comma separated
inline constexpr const char* kStructMemberPin = "member";   // Get Member: which member

void RegisterStructNodes(NodeTypeRegistry& registry, std::vector<std::string> diagramTypes = {});

enum class StructNodeKind { none, make, breakApart, setMembers, getMember };
StructNodeKind StructNodeKindOf(const Node& node);
// The struct a struct node is set to (its "type" pin), or "".
std::string StructNodeType(const Node& node);
// A text setting of a node ("members", "member"), or "".
std::string StructNodeText(const Node& node, const char* pin);
// The pin name a member gets (spaces become _).
std::string StructMemberPinName(const std::string& memberName);
// Names a member cannot have, because struct nodes use them for their own pins: type, value, members, member.
bool StructMemberNameReserved(const std::string& memberName);

// The node's pins follow its struct and choices (above). True if anything changed.
bool SyncStructNodePins(Graph& graph, const NodeTypeRegistry& registry, NodeId node);

// A struct by name: the graph's own first, then the registry's (project, pods, built-in).
const StructDef* FindStructFor(const Graph& graph, const NodeTypeRegistry& registry, const std::string& name);

} // namespace ce::node_system
