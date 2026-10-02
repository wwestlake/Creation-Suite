#include <node_system/frgraph_serialization.h>
#include <node_system/core_control_flow.h>
#include <node_system/monad_nodes.h>
#include <node_system/graph_analysis.h>
#include <node_system/type_registry.h>
#include <node_system/symbol_nodes.h>
#include <node_system/graph_nodes.h>
#include <node_system/flow_nodes.h>
#include <node_system/struct_nodes.h>
#include <node_system/frust_codegen.h>

#include <algorithm>
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

        // Decisions (FLOW.md): Switch and Route, cases named after what drives the selector.
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry reg;
            ns::RegisterSymbolGetNodes(reg);
            reg.RegisterEnum({ "Weather", "Weather", { "Dry", "Wet", "Deep Snow" }, "" });
            ns::RegisterFlowNodes(reg, { ns::StandardFlowType(ns::DataType::Texture), ns::StandardFlowType(ns::DataType::Float) });
            if (reg.Find("core.switch.image") == nullptr || reg.Find("core.route.number") == nullptr
                || reg.Find("core.switch.image")->displayName != "Switch (Image)")
                fail("Flow nodes did not register per type.");

            ns::Graph flow("Flow", ns::GraphTarget::Dataflow);
            auto* sw = ns::AddRegisteredNode(flow, reg, "core.switch.image", &error);
            auto* route = ns::AddRegisteredNode(flow, reg, "core.route.number", &error);
            ns::SyncFlowNodeCases(flow, reg, sw->Id());
            ns::SyncFlowNodeCases(flow, reg, route->Id());
            auto names = [](const ns::Node& n) {
                std::vector<std::string> out;
                for (const auto* p : ns::FlowCasePins(n)) out.push_back(p->name);
                return out;
            };
            if (names(*sw) != std::vector<std::string> { "case_0", "case_1" } || names(*route) != std::vector<std::string> { "case_0", "case_1" }
                || sw->Inputs().back().type.dataType != ns::DataType::Texture || route->Outputs().front().type.dataType != ns::DataType::Float)
                fail("A new flow node does not have two typed cases.");

            // A Choice param in the selector: the cases become Dry, Wet, Deep_Snow.
            ns::Symbol weather { "weather", "Weather", ns::SymbolKind::Param, ns::DataType::Int, std::int64_t { 1 }, "agent", false, "", "Weather" };
            flow.AddSymbol(weather);
            auto* get = ns::AddSymbolGetNode(flow, reg, weather, &error);
            ns::PinId selector = 0;
            for (const auto& p : sw->Inputs()) if (p.name == ns::kFlowSelectorPin) selector = p.id;
            if (! flow.Connect(get->Id(), get->Outputs().front().id, sw->Id(), selector))
                fail("A Choice did not wire into a selector.");
            if (! ns::SyncFlowNodeCases(flow, reg, sw->Id()) || names(*sw) != std::vector<std::string> { "Dry", "Wet", "Deep_Snow" })
                fail("The cases did not take the enum's names.");
            if (ns::SyncFlowNodeCases(flow, reg, sw->Id()))
                fail("Syncing again changed something.");

            // Which case a selector picks: integer n, number floor(n), toggle 0 / 1, clamped.
            if (ns::FlowCaseIndex(std::int64_t { 1 }, 3) != 1 || ns::FlowCaseIndex(2.7f, 3) != 2 || ns::FlowCaseIndex(true, 2) != 1
                || ns::FlowCaseIndex(std::int64_t { 9 }, 3) != 2 || ns::FlowCaseIndex(-1.0f, 3) != 0)
                fail("FlowCaseIndex picked the wrong case.");

            std::vector<std::string> flowErrors;
            if (! ns::ValidateAgainstRegistry(flow, reg, &flowErrors))
                fail("Flow nodes do not validate: " + (flowErrors.empty() ? std::string() : flowErrors.front()));
            const std::string text = ns::SerializeGraph(flow);
            std::string flowError;
            auto reloaded = ns::DeserializeGraph(text, flowError);
            if (reloaded == nullptr || ns::SerializeGraph(*reloaded) != text)
                fail("A graph with flow nodes does not round-trip.");
            std::cout << "NodeSystem flow nodes: ok" << std::endl;
        }

        // Enums made in the node system (owner, 2026-10-01): saved with the graph, used by Choice params and Switch
        // selectors, compiled to FRust enums.
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry reg;
            ns::RegisterSymbolGetNodes(reg);
            ns::RegisterFlowNodes(reg, { ns::StandardFlowType(ns::DataType::Texture) });

            ns::Graph own("OwnEnums", ns::GraphTarget::Dataflow);
            const auto name = ns::MakeEnumName("HSV Channel", own.Enums());
            if (name != "HsvChannel" || ! own.AddEnum({ name, "HSV Channel", { "Hue", "Saturation", "Value" }, "" })
                || own.AddEnum({ name, "Again", {}, "" }) || ns::MakeEnumName("HSV Channel", own.Enums()) != "HsvChannel2")
                fail("Graph enums did not add with a made name.");

            // A Choice param of it drives a Switch: the cases are Hue, Saturation, Value.
            ns::Symbol channel { "channel", "Channel", ns::SymbolKind::Param, ns::DataType::Int, std::int64_t { 1 }, "agent", false, "", "HsvChannel" };
            own.AddSymbol(channel);
            auto* get = ns::AddSymbolGetNode(own, reg, channel, &error);
            auto* sw = ns::AddRegisteredNode(own, reg, "core.switch.image", &error);
            ns::PinId selector = 0;
            for (const auto& p : sw->Inputs()) if (p.name == ns::kFlowSelectorPin) selector = p.id;
            own.Connect(get->Id(), get->Outputs().front().id, sw->Id(), selector);
            ns::SyncFlowNodeCases(own, reg, sw->Id());
            auto names = [](const ns::Node& n) {
                std::vector<std::string> out;
                for (const auto* p : ns::FlowCasePins(n)) out.push_back(p->name);
                return out;
            };
            if (names(*sw) != std::vector<std::string> { "Hue", "Saturation", "Value" })
                fail("A graph enum did not name the Switch's cases.");

            // Renaming a variant renames the case in place: a wire into it stays.
            auto* feed = ns::AddRegisteredNode(own, reg, "core.switch.image", &error); // any image output will do
            ns::SyncFlowNodeCases(own, reg, feed->Id());
            const auto valueCase = ns::FlowCasePins(*sw)[2]->id;
            own.Connect(feed->Id(), feed->Outputs().front().id, sw->Id(), valueCase);
            own.FindEnum("HsvChannel")->variants[2] = "Brightness";
            ns::SyncFlowNodeCases(own, reg, sw->Id());
            if (names(*sw) != std::vector<std::string> { "Hue", "Saturation", "Brightness" } || ns::FlowCasePins(*sw)[2]->id != valueCase
                || own.Connections().size() != 2)
                fail("Renaming a variant lost the case's wire.");
            // Reordering values reorders the cases, each wire staying with its value: Brightness moves first.
            auto& hsvValues = own.FindEnum("HsvChannel")->variants;
            std::rotate(hsvValues.begin(), hsvValues.begin() + 2, hsvValues.end());
            ns::SyncFlowNodeCases(own, reg, sw->Id());
            if (names(*sw) != std::vector<std::string> { "Brightness", "Hue", "Saturation" } || ns::FlowCasePins(*sw)[0]->id != valueCase
                || own.Connections().size() != 2)
                fail("Reordering values did not carry the wires with them.");
            std::rotate(hsvValues.begin(), hsvValues.begin() + 1, hsvValues.end()); // back to Hue, Saturation, Brightness
            ns::SyncFlowNodeCases(own, reg, sw->Id());

            // Saved with the graph, before the symbols that use it, and read back the same.
            const std::string text = ns::SerializeGraph(own);
            if (text.find("enum HsvChannel\nenumname HsvChannel HSV Channel\nenumvariant HsvChannel Hue\nenumvariant HsvChannel Saturation\n"
                          "enumvariant HsvChannel Brightness\nsymbol param channel int") == std::string::npos)
                fail("Unexpected enum lines:\n" + text);
            std::string enumError;
            auto reloaded = ns::DeserializeGraph(text, enumError);
            if (reloaded == nullptr || ns::SerializeGraph(*reloaded) != text || reloaded->FindEnum("HsvChannel") == nullptr
                || ns::FindEnumFor(*reloaded, reg, "HsvChannel")->variants.size() != 3)
                fail("A graph with its own enum does not round-trip.");

            // Compiled to FRust.
            if (ns::FrustEnumDeclarations(own) != "enum HsvChannel { Hue, Saturation, Brightness }\n"
                || ns::FrustEnumDeclaration({ "Weather", "Weather", { "Dry", "deep snow" }, "" }) != "enum Weather { Dry, DeepSnow }")
                fail("Unexpected FRust enum: " + ns::FrustEnumDeclarations(own));
            // Values carry a description and a colour, saved and read back.
            own.FindEnum("HsvChannel")->variants[0].colour = 0xffff4040u;
            own.FindEnum("HsvChannel")->variants[0].description = "Where on the colour wheel";
            const std::string coloured = ns::SerializeGraph(own);
            auto colouredBack = ns::DeserializeGraph(coloured, enumError);
            if (colouredBack == nullptr || colouredBack->FindEnum("HsvChannel")->variants[0].colour != 0xffff4040u
                || colouredBack->FindEnum("HsvChannel")->variants[0].description != "Where on the colour wheel"
                || colouredBack->FindEnum("HsvChannel")->scope != ns::TypeScope::graph)
                fail("An enum value's colour or description did not round-trip.");

            // Project types: a types file in the project scope, swapped into a registry beside its built-in enums.
            ns::NodeTypeRegistry projectRegistry;
            projectRegistry.RegisterEnum({ "Axis", "Axis", { "Horizontal", "Vertical" }, "" });
            const std::string typesText = ns::SerializeTypes({ { "Weather", "Weather", { "Dry", "Wet" }, "" } });
            std::vector<ns::EnumDef> projectEnums;
            std::string typesError;
            if (typesText.rfind("frtypes 1\n", 0) != 0 || ! ns::DeserializeTypes(typesText, ns::TypeScope::project, projectEnums, typesError)
                || projectEnums.size() != 1 || projectEnums[0].scope != ns::TypeScope::project || projectEnums[0].variants[1].name != "Wet")
                fail("A types file did not read back: " + typesError);
            projectRegistry.ReplaceEnums(ns::TypeScope::project, projectEnums);
            if (projectRegistry.FindEnum("Weather") == nullptr || projectRegistry.FindEnum("Axis") == nullptr)
                fail("Project enums did not join the built-in ones.");
            projectRegistry.ReplaceEnums(ns::TypeScope::project, {});
            if (projectRegistry.FindEnum("Weather") != nullptr || projectRegistry.FindEnum("Axis") == nullptr)
                fail("Replacing the project's enums touched the built-in ones.");
            std::cout << "NodeSystem graph enums: ok" << std::endl;
        }

        // Structs (TYPES.md): made in the Struct Editor, used by params, wires and the struct nodes.
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry reg;
            ns::RegisterSymbolGetNodes(reg);
            ns::RegisterStructNodes(reg);
            reg.RegisterEnum({ "BlendMode", "Blend Mode", { "Normal", "Multiply" }, "" });

            ns::Graph g("Structs", ns::GraphTarget::Dataflow);
            ns::StructDef surface;
            surface.name = ns::MakeTypeName("Surface Settings", {});
            surface.displayName = "Surface Settings";
            ns::StructMember strength { "strength", { ns::PinKind::Data, ns::DataType::Float }, 0.5f, "How strong", true, 0.0, 1.0 };
            ns::StructMember tint { "tint", { ns::PinKind::Data, ns::DataType::Color }, ns::Vec3Default { 1.0f, 1.0f, 1.0f }, "", false, 0.0, 1.0 };
            ns::StructMember mode { "mode", { ns::PinKind::Data, ns::DataType::Int }, std::int64_t { 1 }, "", false, 0.0, 1.0 };
            mode.type.enumType = "BlendMode";
            surface.members = { strength, tint, mode };
            if (surface.name != "SurfaceSettings" || ! g.AddStruct(surface) || g.AddStruct(surface) || ns::StructMemberNameReserved("Strength")
                || ! ns::StructMemberNameReserved("value"))
                fail("A struct did not add with a made name, or the reserved names are wrong.");
            if (ns::FrustStructDeclaration(surface) != "struct SurfaceSettings { strength: f64, tint: Array<f64, 3>, mode: BlendMode }")
                fail("Unexpected FRust struct: " + ns::FrustStructDeclaration(surface));

            auto structNode = [&](const char* type, const char* member = nullptr, const char* members = nullptr) {
                auto* n = ns::AddRegisteredNode(g, reg, type, &error);
                for (const auto& p : n->Inputs()) {
                    if (p.name == ns::kStructTypePin) n->FindPin(p.id)->defaultValue = std::string("SurfaceSettings");
                    if (member != nullptr && p.name == ns::kStructMemberPin) n->FindPin(p.id)->defaultValue = std::string(member);
                    if (members != nullptr && p.name == ns::kStructMembersPin) n->FindPin(p.id)->defaultValue = std::string(members);
                }
                ns::SyncStructNodePins(g, reg, n->Id());
                return n;
            };
            auto pinOf = [](ns::Node* n, const std::string& name, bool input) -> ns::PinId {
                for (const auto& p : input ? n->Inputs() : n->Outputs()) if (p.name == name) return p.id;
                return 0;
            };
            auto* make = structNode(ns::kMakeStructType);
            auto* brk = structNode(ns::kBreakStructType);
            auto* set = structNode(ns::kSetMembersType, nullptr, "tint");
            auto* get = structNode(ns::kGetMemberType, "strength");
            if (make->Inputs().size() != 4 || pinOf(make, "mode", true) == 0 || make->Outputs().front().type.structType != "SurfaceSettings"
                || brk->Outputs().size() != 3 || set->Inputs().size() != 4 || pinOf(set, "tint", true) == 0 || pinOf(set, "strength", true) != 0
                || get->Outputs().size() != 1 || get->Outputs().front().name != "strength" || get->Outputs().front().type.dataType != ns::DataType::Float)
                fail("Struct nodes' pins do not follow the struct.");
            if (! g.Connect(make->Id(), pinOf(make, "value", false), brk->Id(), pinOf(brk, "value", true)))
                fail("A struct did not wire into a Break of the same struct.");

            // A struct of another type does not wire in.
            ns::StructDef other { "Other", "Other", { strength }, "", ns::TypeScope::graph };
            g.AddStruct(other);
            auto* otherMake = ns::AddRegisteredNode(g, reg, ns::kMakeStructType, &error);
            for (const auto& p : otherMake->Inputs()) if (p.name == ns::kStructTypePin) otherMake->FindPin(p.id)->defaultValue = std::string("Other");
            ns::SyncStructNodePins(g, reg, otherMake->Id());
            if (g.Connect(otherMake->Id(), pinOf(otherMake, "value", false), set->Id(), pinOf(set, "value", true)))
                fail("A struct of another type wired in.");

            // Renaming a member renames the pins in place: the wire into Make's "strength" stays.
            auto* feed = ns::AddRegisteredNode(g, reg, "core.symbol.get.float", &error);
            g.Connect(feed->Id(), feed->Outputs().front().id, make->Id(), pinOf(make, "strength", true));
            const auto before = g.Connections().size();
            g.FindStruct("SurfaceSettings")->members[0].name = "amount";
            ns::SyncStructNodePins(g, reg, make->Id());
            if (pinOf(make, "amount", true) == 0 || g.Connections().size() != before)
                fail("Renaming a member lost its pin's wire.");
            // Reordering members reorders the pins, the wire staying with "amount".
            auto& surfaceMembers = g.FindStruct("SurfaceSettings")->members;
            std::swap(surfaceMembers[0], surfaceMembers[2]);
            ns::SyncStructNodePins(g, reg, make->Id());
            if (make->Inputs()[1].name != "mode" || make->Inputs()[3].name != "amount" || g.Connections().size() != before)
                fail("Reordering members did not carry the wire with them.");
            std::swap(surfaceMembers[0], surfaceMembers[2]);
            ns::SyncStructNodePins(g, reg, make->Id());

            // A struct param with member values, and its Get node carrying the struct.
            ns::Symbol look { "look", "Look", ns::SymbolKind::Param, ns::DataType::Struct, {}, "agent", false, "", "", "SurfaceSettings",
                              { 0.25f, ns::Vec3Default { 0.5f, 0.25f, 0.0f }, std::int64_t { 0 } } };
            g.AddSymbol(look);
            auto* lookGet = ns::AddSymbolGetNode(g, reg, look, &error);
            if (lookGet == nullptr || lookGet->TypeName() != "core.symbol.get.struct" || lookGet->Outputs().front().type.structType != "SurfaceSettings")
                fail("A struct param's Get node does not carry the struct.");

            std::vector<std::string> structErrors;
            if (! ns::ValidateAgainstRegistry(g, reg, &structErrors))
                fail("Struct nodes do not validate: " + (structErrors.empty() ? std::string() : structErrors.front()));
            const std::string text = ns::SerializeGraph(g);
            std::string structError;
            auto reloaded = ns::DeserializeGraph(text, structError);
            if (text.find("structmember SurfaceSettings float default float 0.5\nstructmembername SurfaceSettings 0 amount\n") == std::string::npos
                || text.find("symbolstruct look SurfaceSettings\nsymbolmember look 0 default float 0.25\n") == std::string::npos)
                fail("Unexpected struct lines:\n" + text);
            if (reloaded == nullptr || ns::SerializeGraph(*reloaded) != text || reloaded->FindStruct("SurfaceSettings")->members[0].maximum != 1.0
                || reloaded->FindSymbol("look")->memberValues.size() != 3)
                fail("A graph with structs does not round-trip: " + structError);

            // A types file with a struct.
            std::vector<ns::EnumDef> fileEnums;
            std::vector<ns::StructDef> fileStructs;
            std::string fileError;
            if (! ns::DeserializeTypes(ns::SerializeTypes({}, { surface }), ns::TypeScope::project, fileEnums, fileStructs, fileError)
                || fileStructs.size() != 1 || fileStructs[0].members.size() != 3 || fileStructs[0].scope != ns::TypeScope::project)
                fail("A types file with a struct did not read back: " + fileError);
            std::cout << "NodeSystem structs: ok" << std::endl;
        }

        // Enums whose values carry data (TYPES.md): sum types in full - a value carries none, one or several values of
        // any type, a struct, or the enum itself (recursion); Make Variant and Match follow the enum.
        {
            namespace ns = ce::node_system;
            ns::NodeTypeRegistry reg;
            ns::RegisterSymbolGetNodes(reg);
            ns::RegisterEnumNodes(reg);
            ns::Graph g("Enums", ns::GraphTarget::Dataflow);

            ns::StructDef company;
            company.name = "Company";
            company.displayName = "Company";
            company.members = { { "year", { ns::PinKind::Data, ns::DataType::Int }, std::int64_t { 2026 }, "", false, 0.0, 1.0 } };
            g.AddStruct(company);
            auto field = [](const char* name, ns::DataType type, const char* enumType = "", const char* structType = "") {
                ns::EnumField f;
                f.name = name;
                f.type = { ns::PinKind::Data, type };
                f.type.enumType = enumType;
                f.type.structType = structType;
                return f;
            };
            ns::EnumDef fill { "Fill", "Fill", { "Nothing", "Solid", "Picture", "Styled" }, "", ns::TypeScope::graph };
            fill.variants[1].fields = { field("colour", ns::DataType::Color), field("amount", ns::DataType::Float) };
            fill.variants[2].fields = { field("image", ns::DataType::Texture) };
            fill.variants[3].fields = { field("company", ns::DataType::Struct, "", "Company") };
            ns::EnumDef list { "List", "List", { "Nil", "Cons" }, "", ns::TypeScope::graph };
            list.variants[1].fields = { field("head", ns::DataType::Float), field("tail", ns::DataType::Int, "List") }; // itself
            g.AddEnum(fill);
            g.AddEnum(list);

            if (ns::FrustEnumDeclaration(fill) != "enum Fill { Nothing, Solid(Array<f64, 3>, f64), Picture(i64), Styled(Company) }"
                || ns::FrustEnumDeclaration(list) != "enum List { Nil, Cons(f64, List) }")
                fail("Unexpected FRust enums: " + ns::FrustEnumDeclaration(fill) + " / " + ns::FrustEnumDeclaration(list));
            if (! ns::EnumCarriesValues(fill) || ns::EnumCarriesValues(ns::EnumDef { "Plain", "Plain", { "A", "B" }, "", ns::TypeScope::graph })
                || ! ns::EnumFieldNameReserved("variant") || ns::EnumFieldNameReserved("amount"))
                fail("EnumCarriesValues or the reserved field names are wrong.");

            auto pinOf = [](ns::Node* n, const std::string& name, bool input) -> ns::PinId {
                for (const auto& p : input ? n->Inputs() : n->Outputs()) if (p.name == name) return p.id;
                return 0;
            };
            auto setText = [](ns::Node* n, const char* pin, const char* text) {
                for (const auto& p : n->Inputs()) if (p.name == pin) n->FindPin(p.id)->defaultValue = std::string(text);
            };
            // Make Variant: Fill, Solid -> inputs type, variant, colour, amount; out a Fill.
            auto* make = ns::AddRegisteredNode(g, reg, ns::kMakeVariantType, &error);
            setText(make, ns::kEnumTypePin, "Fill");
            setText(make, ns::kEnumVariantPin, "Solid");
            ns::SyncEnumNodePins(g, reg, make->Id());
            // Match: Fill -> out Solid_colour, Solid_amount, Picture_image, Styled_company; in a Fill.
            auto* match = ns::AddRegisteredNode(g, reg, ns::kMatchType, &error);
            setText(match, ns::kEnumTypePin, "Fill");
            ns::SyncEnumNodePins(g, reg, match->Id());
            if (make->Inputs().size() != 4 || pinOf(make, "amount", true) == 0 || make->Outputs().front().type.enumType != "Fill"
                || match->Outputs().size() != 4 || match->Outputs()[0].name != "Solid_colour" || match->Outputs()[3].name != "Styled_company"
                || match->Outputs()[3].type.structType != "Company" || match->Inputs()[1].type.enumType != "Fill")
                fail("Make Variant's or Match's pins do not follow the enum.");
            if (! g.Connect(make->Id(), pinOf(make, "value", false), match->Id(), pinOf(match, "value", true)))
                fail("A Fill did not wire from Make Variant into Match.");

            // Renaming a field renames the pins in place: the wire into Make's "amount" stays, Match's pin follows.
            auto* feed = ns::AddRegisteredNode(g, reg, "core.symbol.get.float", &error);
            g.Connect(feed->Id(), feed->Outputs().front().id, make->Id(), pinOf(make, "amount", true));
            const auto before = g.Connections().size();
            g.FindEnum("Fill")->variants[1].fields[1].name = "strength";
            ns::SyncEnumNodePins(g, reg, make->Id());
            ns::SyncEnumNodePins(g, reg, match->Id());
            if (pinOf(make, "strength", true) == 0 || pinOf(match, "Solid_strength", false) == 0 || g.Connections().size() != before)
                fail("Renaming a field lost its pin or its wire.");
            // Another value: Make Variant's inputs become that value's fields (Picture: image).
            setText(make, ns::kEnumVariantPin, "Picture");
            ns::SyncEnumNodePins(g, reg, make->Id());
            if (make->Inputs().size() != 3 || pinOf(make, "image", true) == 0 || make->Inputs()[2].type.dataType != ns::DataType::Texture)
                fail("Make Variant's inputs did not follow its value.");

            // A Choice param of Fill set to Solid, with what Solid carries.
            ns::Symbol look { "look", "Look", ns::SymbolKind::Param, ns::DataType::Int, std::int64_t { 1 }, "agent", false, "", "Fill", "",
                              { ns::Vec3Default { 0.0f, 1.0f, 0.0f }, 0.25f } };
            g.AddSymbol(look);

            std::vector<std::string> enumErrors;
            if (! ns::ValidateAgainstRegistry(g, reg, &enumErrors))
                fail("Enum nodes do not validate: " + (enumErrors.empty() ? std::string() : enumErrors.front()));
            const std::string text = ns::SerializeGraph(g);
            std::string enumError;
            auto reloaded = ns::DeserializeGraph(text, enumError);
            if (text.find("enumvariant Fill Solid\nenumfield Fill 1 color\nenumfieldname Fill 1 0 colour\nenumfield Fill 1 float\n"
                          "enumfieldname Fill 1 1 strength\n") == std::string::npos
                || text.find("enumfield Fill 3 struct struct Company\nenumfieldname Fill 3 0 company\n") == std::string::npos
                || text.find("enumfield List 1 int enum List\nenumfieldname List 1 1 tail\n") == std::string::npos
                || text.find("symbolenum look Fill\nsymbolmember look 0 default vec3 0 1 0\nsymbolmember look 1 default float 0.25\n") == std::string::npos)
                fail("Unexpected enum lines:\n" + text);
            if (reloaded == nullptr || ns::SerializeGraph(*reloaded) != text || reloaded->FindEnum("List")->variants[1].fields[1].type.enumType != "List"
                || reloaded->FindSymbol("look")->memberValues.size() != 2)
                fail("A graph with enums carrying values does not round-trip: " + enumError);

            // A types file keeps what values carry.
            std::vector<ns::EnumDef> fileEnums;
            std::vector<ns::StructDef> fileStructs;
            std::string fileError;
            if (! ns::DeserializeTypes(ns::SerializeTypes({ fill }, {}), ns::TypeScope::project, fileEnums, fileStructs, fileError)
                || fileEnums.size() != 1 || fileEnums[0].variants[1].fields.size() != 2 || fileEnums[0].variants[3].fields[0].type.structType != "Company")
                fail("A types file with enums carrying values did not read back: " + fileError);
            std::cout << "NodeSystem enums carrying values: ok" << std::endl;
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "NodeSystemSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
