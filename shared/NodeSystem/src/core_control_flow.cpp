#include "node_system/core_control_flow.h"

#include <string>
#include <utility>

namespace ce::node_system {

namespace {
PinTypeDesc Exec() { return { PinKind::Exec, DataType::Int }; }
PinTypeDesc Bool() { return { PinKind::Data, DataType::Bool }; }
PinTypeDesc Int() { return { PinKind::Data, DataType::Int }; }
PinSignature In(const char* name, PinTypeDesc type, PinDefaultValue value = {}) {
    return { name, type, std::move(value) };
}
PinSignature Out(const char* name, PinTypeDesc type) { return { name, type, {} }; }
}

void RegisterCoreControlFlowNodes(NodeTypeRegistry& registry)
{
    registry.Register({ "core.branch", Domain::Core,
        { In("execute", Exec()), In("condition", Bool()) },
        { Out("true", Exec()), Out("false", Exec()) }, ControlFlowKind::Branch });

    registry.Register({ "core.sequence", Domain::Core,
        { In("execute", Exec()) },
        { Out("then_0", Exec()), Out("then_1", Exec()) }, ControlFlowKind::Sequence });

    registry.Register({ "core.for", Domain::Core,
        { In("execute", Exec()), In("firstIndex", Int(), std::int64_t(0)),
          In("lastIndex", Int(), std::int64_t(0)), In("step", Int(), std::int64_t(1)) },
        { Out("body", Exec()), Out("completed", Exec()), Out("index", Int()) }, ControlFlowKind::For });

    registry.Register({ "core.while", Domain::Core,
        { In("execute", Exec()), In("condition", Bool()) },
        { Out("body", Exec()), Out("completed", Exec()) }, ControlFlowKind::While });

    registry.Register({ "core.break", Domain::Core, { In("execute", Exec()) }, {}, ControlFlowKind::Break });
    registry.Register({ "core.continue", Domain::Core, { In("execute", Exec()) }, {}, ControlFlowKind::Continue });
    registry.Register({ "core.return", Domain::Core, { In("execute", Exec()) }, {}, ControlFlowKind::Return });
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

} // namespace ce::node_system
