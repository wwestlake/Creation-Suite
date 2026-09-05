#pragma once

#include <string>
#include <vector>

#include "node_system/node_library.h"
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

// Registers the entity-self accessor and the Bool/Int/String Variable
// Get/Set node pairs (Node/Behavior Graph Foundations plan Phase 5) --
// each is a real host-extern call (isHostExtern = true) into
// EngineFrustHost's pod_get/set_variable_* functions, storage-backed by
// the current entity's ObjectState component. Float variables are
// deliberately not included: no dedicated f64-FFI verification exists
// in this codebase yet, unlike bool/i64/String (see EngineFrustHost.h's
// comment on this).
void RegisterCoreVariableNodes(NodeTypeRegistry& registry);

// Registers core.asset.exists (Node/Behavior Graph Foundations plan
// Phase 8) -- a real capability node, host-extern into
// EngineFrustHost::assetExists, declared with requiredCapabilities =
// {"engine.asset.query"} as documentation of intent (this path is
// C++-registered, not loaded from an external plugin manifest, so the
// new capability-enforcement check in EngineNodeLibraryLoader.cpp
// doesn't apply to it -- enforcement matters for untrusted plugin
// content, not this first-party node).
void RegisterCoreCapabilityNodes(NodeTypeRegistry& registry);

// Registers the Domain::Animation host-extern nodes (Animation Control
// plan Phase 2): clip selection/crossfade/speed control and read-only
// query nodes into EngineFrustHost's anim* functions, backed by an
// entity's Animator component. Deliberately i64/string/bool at the FFI
// boundary, not float -- durations/speeds cross as milliseconds/per-mille
// integers, same convention as the pre-existing (not yet node-wrapped)
// engine_set_material_scalar_parameter, since no float-FFI verification
// exists in this codebase (see RegisterCoreVariableNodes' own comment on
// why Float variables aren't supported either).
void RegisterCoreAnimationNodes(NodeTypeRegistry& registry);

// Registers the Domain::Input host-extern query nodes (Input Binding
// System plan): read-only queries into EngineFrustHost's input* functions,
// backed by InputActionSystem's per-tick poll of an active Game's bound
// keyboard/mouse/controller state. No mutating nodes exist -- bindings
// themselves are authored only through InputBindingsPanel, never from a
// graph. i64/string/bool at the FFI boundary, analog values as per-mille
// integers, same convention as RegisterCoreAnimationNodes.
void RegisterCoreInputNodes(NodeTypeRegistry& registry);

// Maps an Event node's typeName to the real FRust lifecycle-hook function
// name a compile pass should emit for it (e.g. "core.event.tick" ->
// "on_tick") -- empty string if typeName isn't a registered Event node.
// The single source of truth for that mapping, so a compile pass never
// hand-duplicates the typeName strings.
std::string EventNodeFrustFunctionName(const std::string& typeName);

// Builds one Domain::Event marker node type per name in `comboNames` --
// same shape as core.event.tick/beginplay/endplay (zero inputs, one Exec
// output named "then", no frustEntryPoint) -- for user-authored Input
// Combo Events (Input Combo Events plan) rather than fixed engine
// lifecycle points. Each type name is "core.input.combo." + name;
// EventNodeFrustFunctionName derives "on_action_<sanitized name>" for
// these dynamically, the same way it looks up a fixed name for the
// built-in three. Pass to NodeLibraryRegistry::ReplaceLibrary (not
// Register) every time the caller's combo list changes, since library ids
// must stay unique under Register's stricter contract.
NodeLibraryDescriptor BuildInputComboEventLibrary(const std::vector<std::string>& comboNames);

} // namespace ce::node_system
