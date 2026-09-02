#pragma once

#include <string>

#include "node_system/type_registry.h"

namespace ce::node_system {

// Registers the Suite's generic execution nodes. The catalog is deliberately
// host-neutral; FRust lowering and host actions are separate concerns.
void RegisterCoreControlFlowNodes(NodeTypeRegistry& registry);

// Registers the exec-output-only Event nodes (On Tick / On Begin Play / On
// End Play) that give an exec chain an explicit, visible start -- Node/
// Behavior Graph Foundations plan Phase 4. See kEventNodeHookNames in
// core_control_flow.cpp for the typeName -> FRust function-name mapping a
// compile pass needs.
void RegisterCoreEventNodes(NodeTypeRegistry& registry);

// Maps an Event node's typeName to the real FRust lifecycle-hook function
// name a compile pass should emit for it (e.g. "core.event.tick" ->
// "on_tick") -- empty string if typeName isn't a registered Event node.
// The single source of truth for that mapping, so a compile pass never
// hand-duplicates the typeName strings.
std::string EventNodeFrustFunctionName(const std::string& typeName);

} // namespace ce::node_system
