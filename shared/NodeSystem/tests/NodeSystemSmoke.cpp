#include <node_system/frgraph_serialization.h>
#include <node_system/core_control_flow.h>
#include <node_system/monad_nodes.h>
#include <node_system/graph_analysis.h>
#include <node_system/type_registry.h>
#include <node_system/symbol_nodes.h>

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

        ce::node_system::NodeTypeRegistry monadRegistry;
        ce::node_system::RegisterCoreMonadNodes(monadRegistry);
        if (monadRegistry.Find("core.wrap.option") == nullptr
            || monadRegistry.Find("core.bind.result") == nullptr)
            fail("Core monad node contracts were not registered.");
        const auto option = monadRegistry.Find("core.wrap.option")->outputs.front().type;
        const auto optionInput = monadRegistry.Find("core.bind.option")->inputs.front().type;
        if (! ce::node_system::IsConnectionCompatible(option, optionInput))
            fail("Option<T> did not match its Bind input.");
        const auto result = monadRegistry.Find("core.wrap.result")->outputs.front().type;
        if (ce::node_system::IsConnectionCompatible(option, result))
            fail("Option<T> was accepted by a Result<T> input.");

        // Graph symbols (params, constants, variables) - SYMBOLS.md.
        {
            namespace ns = ce::node_system;
            ns::Graph symbolGraph("SymbolGraph", ns::GraphTarget::Dataflow);
            const std::string plain = ns::SerializeGraph(symbolGraph);
            if (plain.find("symbol") != std::string::npos)
                fail("A graph without symbols wrote symbol lines.");

            ns::Symbol scale;
            scale.id = ns::MakeSymbolId("Tile Scale", symbolGraph.Symbols());
            scale.name = "Tile Scale";
            scale.kind = ns::SymbolKind::Param;
            scale.type = ns::DataType::Float;
            scale.value = 4.0f;
            scale.accessibility = "agent";
            if (scale.id != "tile_scale" || ! symbolGraph.AddSymbol(scale))
                fail("Could not add a param symbol with a made id.");
            if (ns::MakeSymbolId("Tile Scale", symbolGraph.Symbols()) != "tile_scale_2")
                fail("MakeSymbolId did not avoid an existing id.");
            if (symbolGraph.AddSymbol(scale))
                fail("A duplicate symbol id was accepted.");

            ns::Symbol tint { "tint", "Base Tint", ns::SymbolKind::Constant, ns::DataType::Color, ns::Vec3Default { 0.25f, 0.5f, 0.75f },
                              "graph", false, "" };
            ns::Symbol count { "counter", "Run Counter", ns::SymbolKind::Variable, ns::DataType::Int, std::int64_t { 7 },
                               "private", true, "How many times the graph has run." };
            ns::Symbol label { "label", "Label", ns::SymbolKind::Param, ns::DataType::String, std::string("hello symbol world"),
                               "public", false, "" };
            if (! symbolGraph.AddSymbol(tint) || ! symbolGraph.AddSymbol(count) || ! symbolGraph.AddSymbol(label))
                fail("Could not add constant / variable / text symbols.");

            ns::NodeTypeRegistry symbolRegistry;
            ns::RegisterSymbolGetNodes(symbolRegistry);
            auto* get = ns::AddSymbolGetNode(symbolGraph, symbolRegistry, scale, &error);
            if (get == nullptr || get->TypeName() != "core.symbol.get.float" || ns::SymbolForGetNode(symbolGraph, *get) == nullptr
                || ns::SymbolForGetNode(symbolGraph, *get)->name != "Tile Scale")
                fail("A Get node bound to a symbol does not find it.");

            const std::string text = ns::SerializeGraph(symbolGraph);
            if (text.find("symbol param tile_scale float agent 0 default float 4\n") == std::string::npos)
                fail("Unexpected symbol line:\n" + text);

            std::string symbolError;
            auto reloaded = ns::DeserializeGraph(text, symbolError);
            if (reloaded == nullptr)
                fail("Symbols did not reload: " + symbolError);
            if (reloaded->Symbols() != symbolGraph.Symbols())
                fail("Symbols changed across save and reload.");
            if (ns::SerializeGraph(*reloaded) != text)
                fail("A graph with symbols does not round-trip byte for byte.");
            const auto* reloadedGet = reloaded->FindNode(get->Id());
            if (reloadedGet == nullptr || ns::SymbolForGetNode(*reloaded, *reloadedGet) == nullptr)
                fail("A Get node lost its symbol across save and reload.");
            std::cout << "NodeSystem symbols: ok" << std::endl;
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "NodeSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
