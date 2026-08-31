#pragma once

#include "node_system/type_registry.h"

namespace ce::node_system {

// Registers generic monadic contracts. Pins use Any for the value type so the
// editor/compiler can specialize them during graph type inference.
void RegisterCoreMonadNodes(NodeTypeRegistry& registry);

} // namespace ce::node_system
