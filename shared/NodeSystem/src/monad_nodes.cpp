#include "node_system/monad_nodes.h"

#include <utility>

namespace ce::node_system {

namespace {
PinTypeDesc Value() { return { PinKind::Data, DataType::Any }; }
PinTypeDesc Function() { return { PinKind::Data, DataType::Function }; }
PinTypeDesc Option() { return { PinKind::Data, DataType::Any, { MonadKind::Option, DataType::Any, DataType::Any } }; }
PinTypeDesc Result() { return { PinKind::Data, DataType::Any, { MonadKind::Result, DataType::Any, DataType::Any } }; }
PinSignature In(const char* name, PinTypeDesc type, PinDefaultValue value = {}) { return { name, type, std::move(value) }; }
PinSignature Out(const char* name, PinTypeDesc type) { return { name, type, {} }; }
}

void RegisterCoreMonadNodes(NodeTypeRegistry& registry)
{
    registry.Register({ "core.wrap.option", Domain::Core,
        { In("value", Value()) }, { Out("option", Option()) },
        ControlFlowKind::None, MonadOperation::Wrap });
    registry.Register({ "core.wrap.result", Domain::Core,
        { In("value", Value()) }, { Out("result", Result()) },
        ControlFlowKind::None, MonadOperation::Wrap });

    registry.Register({ "core.bind.option", Domain::Core,
        { In("source", Option()), In("continuation", Function()) },
        { Out("option", Option()) }, ControlFlowKind::None, MonadOperation::Bind });
    registry.Register({ "core.bind.result", Domain::Core,
        { In("source", Result()), In("continuation", Function()) },
        { Out("result", Result()) }, ControlFlowKind::None, MonadOperation::Bind });

    registry.Register({ "core.map.option", Domain::Core,
        { In("source", Option()), In("transform", Function()) },
        { Out("option", Option()) }, ControlFlowKind::None, MonadOperation::Map });
    registry.Register({ "core.map.result", Domain::Core,
        { In("source", Result()), In("transform", Function()) },
        { Out("result", Result()) }, ControlFlowKind::None, MonadOperation::Map });

    registry.Register({ "core.fail", Domain::Core,
        { In("error", Value()) }, { Out("result", Result()) },
        ControlFlowKind::None, MonadOperation::Fail });
    registry.Register({ "core.recover", Domain::Core,
        { In("source", Result()), In("handler", Function()) },
        { Out("result", Result()) }, ControlFlowKind::None, MonadOperation::Recover });
}

} // namespace ce::node_system
