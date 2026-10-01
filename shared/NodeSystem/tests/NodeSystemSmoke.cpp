#include <node_system/frgraph_serialization.h>
#include <node_system/core_control_flow.h>
#include <node_system/monad_nodes.h>
#include <node_system/graph_analysis.h>
#include <node_system/type_registry.h>
#include <node_system/symbol_nodes.h>
#include <node_system/graph_nodes.h>

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

        // Enums: named choices for integer settings, and Choice symbols - SYMBOLS.md "Enums".
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry enumRegistry;
            ns::RegisterSymbolGetNodes(enumRegistry);
            enumRegistry.RegisterEnum({ "BlendMode", "Blend Mode",
                                        { "Normal", "Multiply", "Screen", "Overlay", "Add", "Subtract", "Darken", "Lighten", "Difference" }, "" });
            enumRegistry.RegisterEnum({ "Axis", "Axis", { "Horizontal", "Vertical" }, "" });
            const auto* blendMode = enumRegistry.FindEnum("BlendMode");
            if (blendMode == nullptr || ns::EnumVariantName(*blendMode, 2) != "Screen" || ns::EnumVariantName(*blendMode, 99) != "99")
                fail("Enum variant names are wrong.");

            ns::PinTypeDesc modeType { ns::PinKind::Data, ns::DataType::Int };
            modeType.enumType = "BlendMode";
            ns::PinTypeDesc axisType { ns::PinKind::Data, ns::DataType::Int };
            axisType.enumType = "Axis";
            const ns::PinTypeDesc intType { ns::PinKind::Data, ns::DataType::Int };
            enumRegistry.Register({ "Blend", ns::Domain::Core, { { "mode", modeType, std::int64_t { 0 } } }, {} });
            enumRegistry.Register({ "Ripple", ns::Domain::Core, { { "axis", axisType, std::int64_t { 0 } } }, {} });
            enumRegistry.Register({ "ConstInt", ns::Domain::Core, {}, { { "value", intType, std::int64_t { 3 } } } });

            ns::Graph enumGraph("EnumGraph", ns::GraphTarget::Dataflow);
            ns::Symbol choice { "blend_choice", "Blend Choice", ns::SymbolKind::Param, ns::DataType::Int, std::int64_t { 1 },
                                "agent", false, "", "BlendMode" };
            if (! enumGraph.AddSymbol(choice))
                fail("Could not add a Choice symbol.");
            auto* get = ns::AddSymbolGetNode(enumGraph, enumRegistry, choice, &error);
            auto* blend = ns::AddRegisteredNode(enumGraph, enumRegistry, "Blend", &error);
            auto* ripple = ns::AddRegisteredNode(enumGraph, enumRegistry, "Ripple", &error);
            auto* constant = ns::AddRegisteredNode(enumGraph, enumRegistry, "ConstInt", &error);
            if (get == nullptr || blend == nullptr || ripple == nullptr || constant == nullptr)
                fail("Could not build the enum graph: " + error);
            if (get->Outputs().front().type.enumType != "BlendMode")
                fail("A Choice's Get node did not take its enum.");

            const auto getOut = get->Outputs().front().id;
            if (! enumGraph.Connect(get->Id(), getOut, blend->Id(), blend->Inputs().front().id))
                fail("A Blend Mode choice did not wire into a Blend Mode setting.");
            if (enumGraph.Connect(get->Id(), getOut, ripple->Id(), ripple->Inputs().front().id))
                fail("A Blend Mode choice wired into an Axis setting.");
            if (! enumGraph.Connect(constant->Id(), constant->Outputs().front().id, ripple->Id(), ripple->Inputs().front().id))
                fail("A plain integer did not wire into an enum setting.");
            if (! ns::IsConnectionCompatible(modeType, intType))
                fail("An enum did not wire into a plain integer.");

            std::vector<std::string> enumErrors;
            if (! ns::ValidateAgainstRegistry(enumGraph, enumRegistry, &enumErrors))
                fail("The enum graph does not validate: " + (enumErrors.empty() ? std::string() : enumErrors.front()));

            const std::string text = ns::SerializeGraph(enumGraph);
            const auto blendPinLine = "pin " + std::to_string(blend->Id()) + " in " + std::to_string(blend->Inputs().front().id)
                                    + " mode data int enum BlendMode default int 0\n";
            if (text.find("symbolenum blend_choice BlendMode\n") == std::string::npos || text.find(blendPinLine) == std::string::npos)
                fail("Unexpected enum lines:\n" + text);
            std::string enumError;
            auto reloaded = ns::DeserializeGraph(text, enumError);
            if (reloaded == nullptr)
                fail("The enum graph did not reload: " + enumError);
            if (reloaded->Symbols() != enumGraph.Symbols() || ns::SerializeGraph(*reloaded) != text)
                fail("The enum graph does not round-trip byte for byte.");

            // A pin saved without its tag still finds its enum through the node type.
            auto* untagged = blend->FindPin(blend->Inputs().front().id);
            untagged->type.enumType.clear();
            const auto* found = ns::PinEnum(enumRegistry, *blend, *untagged);
            if (found == nullptr || found->name != "BlendMode")
                fail("PinEnum did not fall back to the node type's signature.");
            std::cout << "NodeSystem enums: ok" << std::endl;
        }

        // Graph types (GRAPH_TYPES.md): the graph's type picks the node types that belong in it.
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry typed;
            ns::RegisterSymbolGetNodes(typed); // shared: no diagram types listed
            typed.RegisterDiagramType({ "image", "Image", "" });
            typed.RegisterDiagramType({ "material", "Material", "" });
            ns::NodeTypeDescriptor blur { "image.blur", ns::Domain::Core, {}, { { "image", { ns::PinKind::Data, ns::DataType::Texture }, {} } } };
            blur.diagramTypes = { "image" };
            ns::NodeTypeDescriptor albedo { "material.albedo", ns::Domain::Core, {}, { { "color", { ns::PinKind::Data, ns::DataType::Color }, {} } } };
            albedo.diagramTypes = { "material" };
            typed.Register(blur);
            typed.Register(albedo);
            const auto* get = typed.Find("core.symbol.get.float");
            if (typed.DiagramTypes().size() != 2 || typed.FindDiagramType("material")->displayName != "Material")
                fail("Diagram types did not register.");
            if (!ns::AllowedInDiagram(blur, "image") || ns::AllowedInDiagram(blur, "material") || !ns::AllowedInDiagram(blur, "")
                || get == nullptr || !ns::AllowedInDiagram(*get, "image") || !ns::AllowedInDiagram(*get, "material"))
                fail("AllowedInDiagram gave the wrong answer.");

            ns::Graph image("Typed", ns::GraphTarget::Dataflow);
            const std::string untyped = ns::SerializeGraph(image);
            if (untyped.find("diagram") != std::string::npos)
                fail("An untyped graph wrote a diagram line.");
            image.SetDiagramType("image");
            auto* b = ns::AddRegisteredNode(image, typed, "image.blur", &error);
            auto* a = ns::AddRegisteredNode(image, typed, "material.albedo", &error);
            if (b == nullptr || a == nullptr)
                fail("Could not build the typed graph.");
            const auto blocked = ns::NodesNotAllowedIn(image, typed, "image");
            if (blocked.size() != 1 || blocked[0] != a->Id())
                fail("NodesNotAllowedIn did not single out the material node.");
            const std::string text = ns::SerializeGraph(image);
            if (text.find("target dataflow\ndiagram image\n") == std::string::npos)
                fail("Unexpected diagram line:\n" + text);
            std::string typedError;
            auto reloaded = ns::DeserializeGraph(text, typedError);
            if (reloaded == nullptr || reloaded->DiagramType() != "image" || ns::SerializeGraph(*reloaded) != text)
                fail("A typed graph does not round-trip byte for byte.");
            std::cout << "NodeSystem graph types: ok" << std::endl;
        }

        // Graphs as nodes (GRAPH_TYPES.md phase 2): a graph's interface, and a Graph node whose pins follow it.
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry reg;
            ns::RegisterGraphNode(reg);
            const ns::PinTypeDesc texture { ns::PinKind::Data, ns::DataType::Texture };
            ns::NodeTypeDescriptor input { "test.input", ns::Domain::Core, { { "name", { ns::PinKind::Data, ns::DataType::String }, std::string() } },
                                           { { "image", texture, {} } } };
            input.graphPort = ns::GraphPort::input;
            ns::NodeTypeDescriptor output { "test.output", ns::Domain::Core,
                                            { { "image", texture, std::string() }, { "name", { ns::PinKind::Data, ns::DataType::String }, std::string() } },
                                            { { "image", texture, {} } } };
            output.graphPort = ns::GraphPort::output;
            reg.Register(input);
            reg.Register(output);

            // The used graph: a param "strength" (Float 0.5), an image input "source", an output "result".
            ns::Graph inner("Inner", ns::GraphTarget::Dataflow);
            inner.AddSymbol({ "strength", "Strength", ns::SymbolKind::Param, ns::DataType::Float, 0.5f, "public", false, "" });
            auto* in = ns::AddRegisteredNode(inner, reg, "test.input", &error);
            auto* out = ns::AddRegisteredNode(inner, reg, "test.output", &error);
            for (const auto& p : in->Inputs()) in->FindPin(p.id)->defaultValue = std::string("source");
            for (const auto& p : out->Inputs()) if (p.name == "name") out->FindPin(p.id)->defaultValue = std::string("result");
            const auto face = ns::InterfaceOf(inner, reg);
            if (face.inputs.size() != 2 || face.inputs[0].name != "strength" || face.inputs[0].type.dataType != ns::DataType::Float
                || ! std::holds_alternative<float>(face.inputs[0].defaultValue) || face.inputs[1].name != "source"
                || face.inputs[1].type.dataType != ns::DataType::Texture || face.outputs.size() != 1 || face.outputs[0].name != "result")
                fail("The graph's interface is not its param, input and output.");

            // A Graph node using it gets the pins; a wire into "source" survives a sync with the same interface and
            // goes when the input is renamed.
            ns::Graph host("Host", ns::GraphTarget::Dataflow);
            auto* user = ns::AddRegisteredNode(host, reg, ns::kGraphNodeType, &error);
            auto* feed = ns::AddRegisteredNode(host, reg, "test.input", &error);
            if (user == nullptr || feed == nullptr || ! ns::SyncGraphNodePins(host, user->Id(), face))
                fail("Could not add a Graph node.");
            if (user->Inputs().size() != 3 || user->Inputs()[1].name != "strength" || user->Outputs().size() != 1 || user->Outputs()[0].name != "result")
                fail("The Graph node's pins do not follow the interface.");
            ns::PinId sourcePin = 0;
            for (const auto& p : user->Inputs()) if (p.name == "source") sourcePin = p.id;
            if (! host.Connect(feed->Id(), feed->Outputs().front().id, user->Id(), sourcePin))
                fail("Could not wire into the Graph node.");
            ns::SyncGraphNodePins(host, user->Id(), face);
            if (host.Connections().size() != 1)
                fail("Syncing the same interface lost a wire.");
            for (const auto& p : in->Inputs()) in->FindPin(p.id)->defaultValue = std::string("photo");
            ns::SyncGraphNodePins(host, user->Id(), ns::InterfaceOf(inner, reg));
            bool hasPhoto = false, hasSource = false;
            for (const auto& p : user->Inputs()) { hasPhoto = hasPhoto || p.name == "photo"; hasSource = hasSource || p.name == "source"; }
            if (! hasPhoto || hasSource || ! host.Connections().empty())
                fail("Renaming the input did not replace the pin and drop its wire.");

            // A Graph node validates against its descriptor (its extra pins vary) and round-trips.
            std::vector<std::string> graphErrors;
            if (! ns::ValidateAgainstRegistry(host, reg, &graphErrors))
                fail("A Graph node does not validate: " + (graphErrors.empty() ? std::string() : graphErrors.front()));
            const std::string text = ns::SerializeGraph(host);
            std::string hostError;
            auto reloaded = ns::DeserializeGraph(text, hostError);
            if (reloaded == nullptr || ns::SerializeGraph(*reloaded) != text)
                fail("A graph with a Graph node does not round-trip.");
            std::cout << "NodeSystem graph nodes: ok" << std::endl;
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "NodeSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
