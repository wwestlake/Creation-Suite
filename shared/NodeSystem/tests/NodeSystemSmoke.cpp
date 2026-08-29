#include <node_system/celg_serialization.h>
#include <node_system/core_control_flow.h>
#include <node_system/graph_analysis.h>
#include <node_system/type_registry.h>

#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        ce::node_system::NodeTypeRegistry registry;
        registry.Register({
            "ConstFloat",
            ce::node_system::Domain::Core,
            {},
            {
                { "value", { ce::node_system::PinKind::Data, ce::node_system::DataType::Float }, 1.0f }
            }
        });
        registry.Register({
            "Add",
            ce::node_system::Domain::Core,
            {
                { "a", { ce::node_system::PinKind::Data, ce::node_system::DataType::Float }, 0.0f },
                { "b", { ce::node_system::PinKind::Data, ce::node_system::DataType::Float }, 0.0f }
            },
            {
                { "sum", { ce::node_system::PinKind::Data, ce::node_system::DataType::Float }, 0.0f }
            }
        });

        ce::node_system::Graph graph("SmokeGraph");
        std::string error;
        auto* left = ce::node_system::AddRegisteredNode(graph, registry, "ConstFloat", &error);
        if (left == nullptr)
            fail("Failed to add left node: " + error);

        auto* right = ce::node_system::AddRegisteredNode(graph, registry, "ConstFloat", &error);
        if (right == nullptr)
            fail("Failed to add right node: " + error);

        auto* add = ce::node_system::AddRegisteredNode(graph, registry, "Add", &error);
        if (add == nullptr)
            fail("Failed to add add node: " + error);

        left->SetEditorPosition(10.0f, 20.0f);
        right->SetEditorPosition(30.0f, 40.0f);
        add->SetEditorPosition(70.0f, 80.0f);

        const auto leftOut = left->Outputs().front().id;
        const auto rightOut = right->Outputs().front().id;
        const auto addInputA = add->Inputs()[0].id;
        const auto addInputB = add->Inputs()[1].id;

        if (! graph.Connect(left->Id(), leftOut, add->Id(), addInputA).has_value())
            fail("Failed to connect left value to add input a.");
        if (! graph.Connect(right->Id(), rightOut, add->Id(), addInputB).has_value())
            fail("Failed to connect right value to add input b.");

        auto validation = ce::node_system::ValidateGraph(graph, &registry);
        if (! validation.ok)
            fail("ValidateGraph unexpectedly failed.");

        const auto serialized = ce::node_system::SerializeGraph(graph);
        std::string deserializeError;
        auto roundTripped = ce::node_system::DeserializeGraph(serialized, deserializeError);
        if (! roundTripped)
            fail("DeserializeGraph failed: " + deserializeError);

        validation = ce::node_system::ValidateGraph(*roundTripped, &registry);
        if (! validation.ok)
            fail("ValidateGraph failed after round-trip.");

        if (roundTripped->Connections().size() != 2)
            fail("Round-tripped graph connection count mismatch.");

        if (! ce::node_system::TopologicalDataOrder(*roundTripped).has_value())
            fail("TopologicalDataOrder failed on a valid graph.");

        if (ce::node_system::DetectExecCycle(*roundTripped).has_value())
            fail("DetectExecCycle reported a cycle where none exists.");

        ce::node_system::NodeTypeRegistry controlRegistry;
        ce::node_system::RegisterCoreControlFlowNodes(controlRegistry);
        ce::node_system::Graph branchGraph("ControlFlowGraph");
        auto* branch = ce::node_system::AddRegisteredNode(branchGraph, controlRegistry, "core.branch", &error);
        if (branch == nullptr || branch->Inputs().size() != 2 || branch->Outputs().size() != 2)
            fail("Branch node contract was not registered correctly.");

        ce::node_system::Graph loopGraph("LoopGraph");
        auto* loop = ce::node_system::AddRegisteredNode(loopGraph, controlRegistry, "core.for", &error);
        if (loop == nullptr)
            fail("For node contract was not registered correctly.");
        const auto findPin = [](const ce::node_system::Node& node, const std::string& name) {
            for (const auto& pin : node.Inputs())
                if (pin.name == name) return pin.id;
            for (const auto& pin : node.Outputs())
                if (pin.name == name) return pin.id;
            return ce::node_system::PinId{};
        };
        if (! loopGraph.Connect(loop->Id(), findPin(*loop, "body"), loop->Id(), findPin(*loop, "execute")).has_value())
            fail("Failed to create the For loop body cycle.");
        if (! ce::node_system::ValidateGraph(loopGraph, &controlRegistry).ok)
            fail("A structured For loop cycle was rejected.");

        ce::node_system::Graph invalidBreakGraph("InvalidBreakGraph");
        if (ce::node_system::AddRegisteredNode(invalidBreakGraph, controlRegistry, "core.break", &error) == nullptr)
            fail("Failed to add Break node.");
        if (ce::node_system::ValidateGraph(invalidBreakGraph, &controlRegistry).ok)
            fail("Break outside a loop was accepted.");

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "NodeSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
