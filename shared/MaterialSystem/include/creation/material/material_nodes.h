#pragma once

#include <string>

#include <node_system/type_registry.h>

namespace ce::material {

// Material graphs share the Suite graph/editor infrastructure, but their
// output is a GPU material evaluation function rather than FRust code.
//
// A material graph is the graph type "material" (shared/NodeSystem/GRAPH_TYPES.md): every material node belongs in
// it, and RegisterMaterialNodes registers the type, so one registry can hold material nodes beside other kinds and
// the palette lists the ones that belong in the graph being edited.
inline constexpr const char* kMaterialDiagram = "material";

void RegisterMaterialNodes(node_system::NodeTypeRegistry& registry);

} // namespace ce::material
