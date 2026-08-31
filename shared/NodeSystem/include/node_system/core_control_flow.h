#pragma once

#include "node_system/type_registry.h"

namespace ce::node_system {

// Registers the Suite's generic execution nodes. The catalog is deliberately
// host-neutral; FRust lowering and host actions are separate concerns.
void RegisterCoreControlFlowNodes(NodeTypeRegistry& registry);

} // namespace ce::node_system
