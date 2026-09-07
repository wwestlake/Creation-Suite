#include "node_system/core_control_flow.h"

#include <cctype>
#include <string>
#include <utility>

namespace ce::node_system {

namespace {
PinTypeDesc Exec() { return { PinKind::Exec, DataType::Int }; }
PinTypeDesc Bool() { return { PinKind::Data, DataType::Bool }; }
PinTypeDesc Int() { return { PinKind::Data, DataType::Int }; }
PinTypeDesc Float() { return { PinKind::Data, DataType::Float }; }
PinTypeDesc Str() { return { PinKind::Data, DataType::String }; }
PinTypeDesc EntityType() { return { PinKind::Data, DataType::Entity }; }
PinSignature In(const char* name, PinTypeDesc type, PinDefaultValue value = {}) {
    return { name, type, std::move(value) };
}
PinSignature Out(const char* name, PinTypeDesc type) { return { name, type, {} }; }
}

namespace {
// Named-field construction, not positional brace-init: NodeTypeDescriptor's
// member order has already been reordered once by a later phase (see
// FrustCodegenSmoke.cpp's own comment on the identical class of bug found
// there) and gained another field since (genericParams) -- positional
// init is fragile against exactly this, named-field isn't.
void RegisterControlFlowNode(NodeTypeRegistry& registry, std::string typeName, std::string displayName,
                              std::vector<PinSignature> inputs, std::vector<PinSignature> outputs,
                              ControlFlowKind controlFlow) {
    NodeTypeDescriptor descriptor;
    descriptor.typeName = std::move(typeName);
    descriptor.domain = Domain::Core;
    descriptor.inputs = std::move(inputs);
    descriptor.outputs = std::move(outputs);
    descriptor.controlFlow = controlFlow;
    descriptor.displayName = std::move(displayName);
    registry.Register(std::move(descriptor));
}
}

void RegisterCoreControlFlowNodes(NodeTypeRegistry& registry)
{
    RegisterControlFlowNode(registry, "core.branch", "Branch",
        { In("execute", Exec()), In("condition", Bool()) },
        { Out("true", Exec()), Out("false", Exec()) }, ControlFlowKind::Branch);

    RegisterControlFlowNode(registry, "core.sequence", "Sequence",
        { In("execute", Exec()) },
        { Out("then_0", Exec()), Out("then_1", Exec()) }, ControlFlowKind::Sequence);

    RegisterControlFlowNode(registry, "core.for", "For Loop",
        { In("execute", Exec()), In("firstIndex", Int(), std::int64_t(0)),
          In("lastIndex", Int(), std::int64_t(0)), In("step", Int(), std::int64_t(1)) },
        { Out("body", Exec()), Out("completed", Exec()), Out("index", Int()) }, ControlFlowKind::For);

    RegisterControlFlowNode(registry, "core.while", "While Loop",
        { In("execute", Exec()), In("condition", Bool()) },
        { Out("body", Exec()), Out("completed", Exec()) }, ControlFlowKind::While);

    RegisterControlFlowNode(registry, "core.break", "Break", { In("execute", Exec()) }, {}, ControlFlowKind::Break);
    RegisterControlFlowNode(registry, "core.continue", "Continue", { In("execute", Exec()) }, {}, ControlFlowKind::Continue);
    RegisterControlFlowNode(registry, "core.return", "Return", { In("execute", Exec()) }, {}, ControlFlowKind::Return);
}

namespace {
// typeName, FRust lifecycle-hook function name (must match the real hook
// names EngineFrustHost.cpp invokes -- on_tick/on_begin_play/on_end_play).
// Single source of truth for both registration below and
// EventNodeFrustFunctionName().
constexpr std::pair<const char*, const char*> kEventNodeHooks[] = {
    { "core.event.tick", "on_tick" },
    { "core.event.beginplay", "on_begin_play" },
    { "core.event.endplay", "on_end_play" },
    { "core.physics.onCollisionEnter", "on_collision_enter" },
    { "core.physics.onCollisionStay", "on_collision_stay" },
    { "core.physics.onCollisionExit", "on_collision_exit" },
};
}

void RegisterCoreEventNodes(NodeTypeRegistry& registry)
{
    // Zero inputs, one Exec output -- nothing feeds these, they ARE the
    // start of an exec chain. Gives execution an explicit, visible entry
    // point instead of the old "first node with an unwired Exec input"
    // auto-detect heuristic (found missing during the Pod plan's
    // post-implementation verification pass).
    auto registerEventNode = [&](std::string typeName, std::string displayName) {
        NodeTypeDescriptor descriptor;
        descriptor.typeName = std::move(typeName);
        descriptor.domain = Domain::Event;
        descriptor.outputs = { Out("then", Exec()) };
        descriptor.displayName = std::move(displayName);
        registry.Register(std::move(descriptor));
    };
    registerEventNode("core.event.tick", "On Tick");
    registerEventNode("core.event.beginplay", "On Begin Play");
    registerEventNode("core.event.endplay", "On End Play");
    // Jolt vendoring plan (Decision 5) -- fixed three, same shape as the
    // lifecycle events above, not a per-collision-pair dynamic set (unlike
    // Input Combo Events' BuildInputComboEventLibrary below, there's no
    // enumerable "list of collisions" to build one marker per name from).
    registerEventNode("core.physics.onCollisionEnter", "On Collision Enter");
    registerEventNode("core.physics.onCollisionStay", "On Collision Stay");
    registerEventNode("core.physics.onCollisionExit", "On Collision Exit");
}

namespace {
// core.input.combo.<name> -> on_action_<sanitized name> -- the one
// convention-based rule EventNodeFrustFunctionName needs beyond the fixed
// kEventNodeHooks table, so a dynamically user-named Input Combo Event
// (Input Combo Events plan) compiles as its own real hook the same way
// the three built-in lifecycle events do, with zero changes needed to
// PodEditorPanel.cpp's own two call sites (both already just call this
// function) or to frust_codegen.cpp's lowerExecChain (already generic
// over any Domain::Event node's shape).
constexpr const char* kComboTypeNamePrefix = "core.input.combo.";

std::string SanitizeFrustIdentifier(const std::string& raw)
{
    std::string result;
    result.reserve(raw.size());
    for (const char c : raw)
        result += (std::isalnum(static_cast<unsigned char>(c)) || c == '_') ? c : '_';
    if (result.empty() || std::isdigit(static_cast<unsigned char>(result.front())))
        result.insert(result.begin(), '_');
    return result;
}
} // namespace

std::string EventNodeFrustFunctionName(const std::string& typeName)
{
    for (const auto& [name, hook] : kEventNodeHooks)
        if (typeName == name) return hook;
    if (typeName.rfind(kComboTypeNamePrefix, 0) == 0)
        return "on_action_" + SanitizeFrustIdentifier(typeName.substr(std::string(kComboTypeNamePrefix).size()));
    return {};
}

NodeLibraryDescriptor BuildInputComboEventLibrary(const std::vector<std::string>& comboNames)
{
    NodeLibraryDescriptor library;
    library.id = "input-combos";
    library.displayName = "Input Combo Events";
    library.description = "User-recorded key-combo events (Input Combo Events plan) -- one Domain::Event marker node per named combo, same shape as core.event.tick/beginplay/endplay.";
    library.target = GraphTarget::Behavior;
    for (const auto& name : comboNames) {
        NodeTypeDescriptor descriptor;
        descriptor.typeName = kComboTypeNamePrefix + name;
        descriptor.domain = Domain::Event;
        descriptor.outputs = { Out("then", Exec()) };
        descriptor.displayName = "Combo: " + name;
        library.nodeTypes.push_back(std::move(descriptor));
    }
    return library;
}

namespace {
// Field count needed (frustEntryPoint, isHostExtern) makes positional
// brace-init unreadable -- named fields instead, one host-extern node
// type at a time.
void RegisterHostExternNode(NodeTypeRegistry& registry, std::string typeName, std::string displayName,
                             std::vector<PinSignature> inputs, std::vector<PinSignature> outputs,
                             std::string frustEntryPoint, std::vector<std::string> requiredCapabilities = {},
                             Domain domain = Domain::Core) {
    NodeTypeDescriptor descriptor;
    descriptor.typeName = std::move(typeName);
    descriptor.domain = domain;
    descriptor.inputs = std::move(inputs);
    descriptor.outputs = std::move(outputs);
    descriptor.displayName = std::move(displayName);
    descriptor.frustEntryPoint = std::move(frustEntryPoint);
    descriptor.isHostExtern = true;
    descriptor.requiredCapabilities = std::move(requiredCapabilities);
    registry.Register(std::move(descriptor));
}

// One Get/Set pair for a supported Variable value type -- name is the
// per-instance literal (matches the "GetVariable's variable name is
// deliberately a per-instance literal" convention already documented on
// PinSignature above), entity/pod_id are real wired inputs so the
// generic pure-node call-emission pass needs no special-casing at all;
// entity is meant to be wired from a core.entity.self node, pod_id from
// a core.literal.string carrying this Pod's own name.
void RegisterVariablePair(NodeTypeRegistry& registry, const char* suffix, const char* label, PinTypeDesc valueType,
                           const char* getHostFn, const char* setHostFn) {
    RegisterHostExternNode(registry, std::string("core.variable.get.") + suffix, std::string("Get ") + label + " Variable",
        { In("entity", EntityType()), In("pod_id", Str()), In("name", Str()) },
        { Out("value", valueType) }, getHostFn);
    RegisterHostExternNode(registry, std::string("core.variable.set.") + suffix, std::string("Set ") + label + " Variable",
        { In("entity", EntityType()), In("pod_id", Str()), In("name", Str()), In("value", valueType) },
        { Out("ok", Int()) }, setHostFn);
}
}

void RegisterCoreVariableNodes(NodeTypeRegistry& registry)
{
    RegisterHostExternNode(registry, "core.entity.self", "Self Entity", {}, { Out("entity", EntityType()) },
        "engine_current_object_entity");
    // Cross-entity reference by placed-instance name: a button Pod acting on
    // a separately-placed door needs an entity reference to something that
    // ISN'T core.entity.self. Resolves ce::scene::Name (set when an object is
    // placed/renamed in the Hierarchy) to an entity id; entity == -1 if no
    // placed instance currently carries that name.
    RegisterHostExternNode(registry, "core.entity.findByName", "Find Entity By Name",
        { In("name", Str()) }, { Out("entity", EntityType()) },
        "engine_entity_find_by_name");
    // The real drag-onto-graph capability core.entity.findByName's own
    // comment named as still-future work: dragging a placed scene instance
    // onto a Pod's graph creates one of these, with instanceId's literal
    // baked to that instance's stable scene::InstanceId (not its mutable,
    // non-unique Name) at drop time. entity == -1 if no live entity
    // currently carries that InstanceId (e.g. it was deleted since the Pod
    // last saved).
    RegisterHostExternNode(registry, "core.entity.findByInstanceId", "Scene Instance Reference",
        { In("instanceId", Str()) }, { Out("entity", EntityType()) },
        "engine_entity_find_by_instance_id");
    RegisterVariablePair(registry, "bool", "Bool", Bool(), "pod_get_variable_bool", "pod_set_variable_bool");
    RegisterVariablePair(registry, "int", "Int", Int(), "pod_get_variable_int", "pod_set_variable_int");
    RegisterVariablePair(registry, "string", "String", Str(), "pod_get_variable_string", "pod_set_variable_string");
}

void RegisterCoreCapabilityNodes(NodeTypeRegistry& registry)
{
    RegisterHostExternNode(registry, "core.asset.exists", "Asset Exists",
        { In("name", Str()) }, { Out("exists", Bool()) }, "engine_asset_exists",
        { "engine.asset.query" });
}

void RegisterCoreAnimationNodes(NodeTypeRegistry& registry)
{
    // Hard cut to `clipName`, no blend -- use core.anim.crossfadeTo for a
    // smooth transition instead.
    RegisterHostExternNode(registry, "core.anim.setActiveClip", "Set Active Clip",
        { In("entity", EntityType()), In("clipName", Str()) }, { Out("ok", Int()) },
        "engine_anim_set_active_clip", {}, Domain::Animation);

    RegisterHostExternNode(registry, "core.anim.crossfadeTo", "Crossfade To Clip",
        { In("entity", EntityType()), In("clipName", Str()), In("blendMillis", Int()) }, { Out("ok", Int()) },
        "engine_anim_crossfade_to", {}, Domain::Animation);

    // speedPerMille: 1000 = normal speed, 2000 = 2x, 500 = 0.5x.
    RegisterHostExternNode(registry, "core.anim.setPlaybackSpeed", "Set Playback Speed",
        { In("entity", EntityType()), In("speedPerMille", Int()) }, { Out("ok", Int()) },
        "engine_anim_set_playback_speed_permille", {}, Domain::Animation);

    RegisterHostExternNode(registry, "core.anim.getActiveClipName", "Get Active Clip Name",
        { In("entity", EntityType()) }, { Out("clipName", Str()) },
        "engine_anim_get_active_clip_name", {}, Domain::Animation);

    RegisterHostExternNode(registry, "core.anim.getClipDuration", "Get Clip Duration (ms)",
        { In("entity", EntityType()), In("clipName", Str()) }, { Out("durationMillis", Int()) },
        "engine_anim_get_clip_duration_millis", {}, Domain::Animation);

    // So a locomotion Pod can avoid re-triggering a crossfade that's
    // already in progress toward the same target clip.
    RegisterHostExternNode(registry, "core.anim.isBlending", "Is Blending",
        { In("entity", EntityType()) }, { Out("isBlending", Bool()) },
        "engine_anim_is_blending", {}, Domain::Animation);
}

void RegisterCoreInputNodes(NodeTypeRegistry& registry)
{
    // No `entity` input on any of these -- Action state is process-global
    // (one poll per tick), matching engine::GameplayInput's own shape, not
    // a new convention.
    RegisterHostExternNode(registry, "core.input.isActionActive", "Is Action Active",
        { In("action", Str()) }, { Out("active", Bool()) },
        "engine_input_is_action_active", {}, Domain::Input);

    RegisterHostExternNode(registry, "core.input.wasActionPressed", "Was Action Pressed",
        { In("action", Str()) }, { Out("pressed", Bool()) },
        "engine_input_was_action_pressed", {}, Domain::Input);

    RegisterHostExternNode(registry, "core.input.wasActionReleased", "Was Action Released",
        { In("action", Str()) }, { Out("released", Bool()) },
        "engine_input_was_action_released", {}, Domain::Input);

    // valuePerMille: 1000 = full magnitude, matching the setPlaybackSpeed/
    // blendMillis integer-encoding convention already used for analog-ish
    // values elsewhere in this file.
    RegisterHostExternNode(registry, "core.input.getActionValue", "Get Action Value",
        { In("action", Str()) }, { Out("valuePerMille", Int()) },
        "engine_input_get_action_value_permille", {}, Domain::Input);
}

void RegisterCorePhysicsNodes(NodeTypeRegistry& registry)
{
    // motionType: 0 = Static, 1 = Kinematic, 2 = Dynamic (ce::physics::MotionType).
    // Config node -- does not create the Jolt body itself; see the header
    // comment above RegisterCorePhysicsNodes.
    RegisterHostExternNode(registry, "core.physics.setRigidBody", "Rigid Body",
        { In("entity", EntityType()), In("motionType", Int()), In("mass", Float()),
          In("friction", Float()), In("restitution", Float()),
          In("linearDamping", Float()), In("angularDamping", Float()) },
        { Out("ok", Int()) }, "engine_physics_set_rigid_body");

    // shapeKind: 0 = Box, 1 = Sphere, 2 = Capsule (ce::physics::ColliderShapeKind).
    // halfExtentX/Y/Z apply to Box; radius to Sphere/Capsule; halfHeight to
    // Capsule only -- unused fields for a given shape are simply ignored.
    // isSensor (Possessable Designer Character plan, Phase 2): a sensor
    // collider fires the same on_collision* hooks but applies no physical
    // collision response -- Jolt's own BodyCreationSettings::mIsSensor,
    // "can be used as a trigger volume." The button/door interaction case.
    RegisterHostExternNode(registry, "core.physics.setColliderShape", "Collider Shape",
        { In("entity", EntityType()), In("shapeKind", Int()),
          In("halfExtentX", Float()), In("halfExtentY", Float()), In("halfExtentZ", Float()),
          In("radius", Float()), In("halfHeight", Float()), In("collisionLayer", Int()),
          In("isSensor", Bool()) },
        { Out("ok", Int()) }, "engine_physics_set_collider_shape");

    RegisterHostExternNode(registry, "core.physics.applyForce", "Apply Force",
        { In("entity", EntityType()), In("x", Float()), In("y", Float()), In("z", Float()) },
        { Out("ok", Int()) }, "engine_physics_apply_force");

    RegisterHostExternNode(registry, "core.physics.applyImpulse", "Apply Impulse",
        { In("entity", EntityType()), In("x", Float()), In("y", Float()), In("z", Float()) },
        { Out("ok", Int()) }, "engine_physics_apply_impulse");

    RegisterHostExternNode(registry, "core.physics.setLinearVelocity", "Set Linear Velocity",
        { In("entity", EntityType()), In("x", Float()), In("y", Float()), In("z", Float()) },
        { Out("ok", Int()) }, "engine_physics_set_linear_velocity");

    RegisterHostExternNode(registry, "core.physics.getLinearVelocityX", "Get Linear Velocity X",
        { In("entity", EntityType()) }, { Out("x", Float()) }, "engine_physics_get_linear_velocity_x");
    RegisterHostExternNode(registry, "core.physics.getLinearVelocityY", "Get Linear Velocity Y",
        { In("entity", EntityType()) }, { Out("y", Float()) }, "engine_physics_get_linear_velocity_y");
    RegisterHostExternNode(registry, "core.physics.getLinearVelocityZ", "Get Linear Velocity Z",
        { In("entity", EntityType()) }, { Out("z", Float()) }, "engine_physics_get_linear_velocity_z");

    // A single cached query -- one FFI call can only return one scalar, so
    // this performs the raycast and caches the result; the four getters
    // below just read that cache. Call this first, then read whichever of
    // the getters you need, same tick.
    RegisterHostExternNode(registry, "core.physics.raycast", "Raycast",
        { In("originX", Float()), In("originY", Float()), In("originZ", Float()),
          In("dirX", Float()), In("dirY", Float()), In("dirZ", Float()), In("maxDistance", Float()) },
        { Out("hit", Bool()) }, "engine_physics_raycast");
    RegisterHostExternNode(registry, "core.physics.raycastHitEntity", "Raycast Hit Entity",
        {}, { Out("entity", EntityType()) }, "engine_physics_raycast_hit_entity");
    RegisterHostExternNode(registry, "core.physics.raycastHitDistance", "Raycast Hit Distance",
        {}, { Out("distance", Float()) }, "engine_physics_raycast_hit_distance");
    RegisterHostExternNode(registry, "core.physics.raycastNormalX", "Raycast Hit Normal X",
        {}, { Out("x", Float()) }, "engine_physics_raycast_normal_x");
    RegisterHostExternNode(registry, "core.physics.raycastNormalY", "Raycast Hit Normal Y",
        {}, { Out("y", Float()) }, "engine_physics_raycast_normal_y");
    RegisterHostExternNode(registry, "core.physics.raycastNormalZ", "Raycast Hit Normal Z",
        {}, { Out("z", Float()) }, "engine_physics_raycast_normal_z");
}

} // namespace ce::node_system
