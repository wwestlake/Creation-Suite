#include "node_system/core_control_flow.h"

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

} // namespace ce::node_system
