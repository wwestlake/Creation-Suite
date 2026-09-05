#include "node_system/core_control_flow.h"

#include <string>
#include <utility>

namespace ce::node_system {

namespace {
PinTypeDesc Exec() { return { PinKind::Exec, DataType::Int }; }
PinTypeDesc Bool() { return { PinKind::Data, DataType::Bool }; }
PinTypeDesc Int() { return { PinKind::Data, DataType::Int }; }
PinTypeDesc Str() { return { PinKind::Data, DataType::String }; }
PinTypeDesc EntityType() { return { PinKind::Data, DataType::Entity }; }
PinSignature In(const char* name, PinTypeDesc type, PinDefaultValue value = {}) {
    return { name, type, std::move(value) };
}
PinSignature Out(const char* name, PinTypeDesc type) { return { name, type, {} }; }
}

void RegisterCoreControlFlowNodes(NodeTypeRegistry& registry)
{
    // displayName (7th positional field, MonadOperation::None as the 6th
    // default) added Phase 7 -- content-clarity pass, so the Inspector
    // shows "Branch" instead of the raw "core.branch".
    registry.Register({ "core.branch", Domain::Core,
        { In("execute", Exec()), In("condition", Bool()) },
        { Out("true", Exec()), Out("false", Exec()) }, ControlFlowKind::Branch, MonadOperation::None, "Branch" });

    registry.Register({ "core.sequence", Domain::Core,
        { In("execute", Exec()) },
        { Out("then_0", Exec()), Out("then_1", Exec()) }, ControlFlowKind::Sequence, MonadOperation::None, "Sequence" });

    registry.Register({ "core.for", Domain::Core,
        { In("execute", Exec()), In("firstIndex", Int(), std::int64_t(0)),
          In("lastIndex", Int(), std::int64_t(0)), In("step", Int(), std::int64_t(1)) },
        { Out("body", Exec()), Out("completed", Exec()), Out("index", Int()) }, ControlFlowKind::For, MonadOperation::None, "For Loop" });

    registry.Register({ "core.while", Domain::Core,
        { In("execute", Exec()), In("condition", Bool()) },
        { Out("body", Exec()), Out("completed", Exec()) }, ControlFlowKind::While, MonadOperation::None, "While Loop" });

    registry.Register({ "core.break", Domain::Core, { In("execute", Exec()) }, {}, ControlFlowKind::Break, MonadOperation::None, "Break" });
    registry.Register({ "core.continue", Domain::Core, { In("execute", Exec()) }, {}, ControlFlowKind::Continue, MonadOperation::None, "Continue" });
    registry.Register({ "core.return", Domain::Core, { In("execute", Exec()) }, {}, ControlFlowKind::Return, MonadOperation::None, "Return" });
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
};
}

void RegisterCoreEventNodes(NodeTypeRegistry& registry)
{
    // Zero inputs, one Exec output -- nothing feeds these, they ARE the
    // start of an exec chain. Gives execution an explicit, visible entry
    // point instead of the old "first node with an unwired Exec input"
    // auto-detect heuristic (found missing during the Pod plan's
    // post-implementation verification pass).
    registry.Register({ "core.event.tick", Domain::Event, {}, { Out("then", Exec()) },
        ControlFlowKind::None, MonadOperation::None, "On Tick" });
    registry.Register({ "core.event.beginplay", Domain::Event, {}, { Out("then", Exec()) },
        ControlFlowKind::None, MonadOperation::None, "On Begin Play" });
    registry.Register({ "core.event.endplay", Domain::Event, {}, { Out("then", Exec()) },
        ControlFlowKind::None, MonadOperation::None, "On End Play" });
}

std::string EventNodeFrustFunctionName(const std::string& typeName)
{
    for (const auto& [name, hook] : kEventNodeHooks)
        if (typeName == name) return hook;
    return {};
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

} // namespace ce::node_system
