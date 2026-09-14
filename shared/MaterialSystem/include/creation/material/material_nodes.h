#pragma once

#include <string>

#include <node_system/type_registry.h>

namespace ce::material {

// Material graphs share the Suite graph/editor infrastructure, but their
// output is a GPU material evaluation function rather than FRust code.
void RegisterMaterialNodes(node_system::NodeTypeRegistry& registry);

} // namespace ce::material
