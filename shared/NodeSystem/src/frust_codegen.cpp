#include "node_system/frust_codegen.h"

#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <sstream>

#include "node_system/graph_analysis.h"

namespace ce::node_system {
namespace {

std::string FrustType(DataType type) {
    switch (type) {
    case DataType::Float: return "f64";
    case DataType::Bool: return "bool";
    case DataType::Int: return "i64";
    case DataType::String: return "String";
    // Opaque reference handles (Node/Behavior Graph Foundations plan,
    // Phase 3) -- all cross the FRust ABI as plain i64, same proven shape
    // EngineLifecycle.frust already uses for Entity references. Type
    // safety for these lives at the graph/NodeSystem layer
    // (IsConnectionCompatible), not in FRust's own type system.
    case DataType::Entity:
    case DataType::Transform:
    case DataType::Material:
    case DataType::Model:
    case DataType::Controller: return "i64";
    default: return {};
    }
}

std::string EscapeString(const std::string& value) {
    std::string result;
    for (const char c : value) {
        if (c == '\\' || c == '\"') result += '\\';
        result += c;
    }
    return result;
}

std::string DefaultExpression(const Pin& pin) {
    if (const auto* value = std::get_if<std::int64_t>(&pin.defaultValue)) return std::to_string(*value);
    if (const auto* value = std::get_if<float>(&pin.defaultValue)) return std::to_string(*value);
    if (const auto* value = std::get_if<bool>(&pin.defaultValue)) return *value ? "true" : "false";
    if (const auto* value = std::get_if<std::string>(&pin.defaultValue)) return "\"" + EscapeString(*value) + "\"";
    return {};
}

const Connection* InputConnection(const Graph& graph, NodeId node, PinId pin) {
    for (const Connection& connection : graph.Connections())
        if (connection.toNode == node && connection.toPin == pin) return &connection;
    return nullptr;
}

const Connection* ExecThenConnection(const Graph& graph, const Node& node) {
    const auto output = std::find_if(node.Outputs().begin(), node.Outputs().end(), [](const Pin& pin) {
        return pin.type.kind == PinKind::Exec && pin.name == "then";
    });
    if (output == node.Outputs().end()) return nullptr;
    for (const Connection& connection : graph.Connections())
        if (connection.fromNode == node.Id() && connection.fromPin == output->id) return &connection;
    return nullptr;
}

// Generalizes ExecThenConnection to any named Exec output -- control-flow
// nodes have several (Branch's "true"/"false", For's "body"/"completed",
// ...), not just the single "then" ordinary callable nodes use.
const Connection* ExecConnectionFromOutputNamed(const Graph& graph, const Node& node, const char* outputName) {
    const auto output = std::find_if(node.Outputs().begin(), node.Outputs().end(), [&](const Pin& pin) {
        return pin.type.kind == PinKind::Exec && pin.name == outputName;
    });
    if (output == node.Outputs().end()) return nullptr;
    for (const Connection& connection : graph.Connections())
        if (connection.fromNode == node.Id() && connection.fromPin == output->id) return &connection;
    return nullptr;
}

const Pin* InputPinNamed(const Node& node, const char* name) {
    const auto it = std::find_if(node.Inputs().begin(), node.Inputs().end(),
                                  [&](const Pin& pin) { return pin.name == name; });
    return it == node.Inputs().end() ? nullptr : &(*it);
}

} // namespace

std::string EscapeFrustString(const std::string& value) { return EscapeString(value); }

FrustGraphCompileResult CompileBehaviorGraphToFrust(const Graph& graph,
                                                     const NodeLibraryRegistry& libraries,
                                                     const FrustGraphCompileOptions& options) {
    FrustGraphCompileResult result;
    // Dataflow graphs (Processing Pods, Pod Management System plan Phase 7)
    // go through this exact same compile path -- a Dataflow graph simply
    // has no natural entryNode, which the existing "no-op if entryNode ==
    // 0" handling below already covers correctly. Material graphs are the
    // only target genuinely out of scope here (ce::material::
    // CompileMaterialGraph is the compiler for that target).
    if (graph.Target() != GraphTarget::Behavior && graph.Target() != GraphTarget::Dataflow) {
        result.error = "only behavior or dataflow graphs compile to FRust";
        return result;
    }
    // A result (data output) and an entry (exec chain start) are two
    // independent reasons to compile a function -- an Event-triggered
    // chain (Node/Behavior Graph Foundations plan Phase 4) has an
    // entryNode and legitimately no resultNode at all, same as
    // core_trigger()'s own callable-with-a-trivial-return shape. At least
    // one of the two must be present; requiring resultNode specifically
    // predates entryNode/control-flow support and was never relaxed.
    if (options.functionName.empty() || (options.resultNode == 0 && options.entryNode == 0)) {
        result.error = "a function name and a result node/pin or an entry node are required";
        return result;
    }
    const ValidationResult validation = ValidateGraph(graph, &libraries.TypeRegistry());
    if (!validation.ok) {
        result.error = validation.errors.empty() ? "graph validation failed" : validation.errors.front();
        return result;
    }
    const auto order = TopologicalDataOrder(graph);
    if (!order) {
        result.error = "data dependency cycle detected";
        return result;
    }

    std::map<std::pair<NodeId, PinId>, std::string> parameterBindings;
    std::map<std::string, DataType> parameterTypes;
    for (const auto& parameter : options.parameters) {
        if (parameter.name.empty() || FrustType(parameter.type).empty() || !parameterTypes.emplace(parameter.name, parameter.type).second) {
            result.error = "graph parameters must have unique names and FRust-supported types";
            return result;
        }
    }
    for (const auto& binding : options.inputBindings) {
        if (!parameterTypes.contains(binding.parameterName) ||
            !parameterBindings.emplace(std::pair { binding.node, binding.pin }, binding.parameterName).second) {
            result.error = "each graph input binding must name one declared parameter";
            return result;
        }
    }

    std::ostringstream body;
    std::set<std::string, std::less<>> importedModules(options.sourceModules.begin(), options.sourceModules.end());

    // Nodes that got a `let n{id} = ...` binding below -- resolveInputExpr
    // refuses to reference anything NOT in this set, which is what turns
    // "this input depends on a control-flow-scoped value" (e.g. a For
    // loop's own index) into an honest compile error instead of emitting a
    // reference to a FRust variable that was never declared. See the
    // structured lowering below for why: control-flow nodes are excluded
    // from this flat pass entirely and computed inline instead.
    std::set<NodeId> boundDataNodes;

    auto importNodeModules = [&](const NodeTypeDescriptor& type) {
        for (const auto& [libraryId, library] : libraries.Libraries()) {
            (void) libraryId;
            if (std::any_of(library.nodeTypes.begin(), library.nodeTypes.end(), [&](const NodeTypeDescriptor& candidate) {
                    return candidate.typeName == type.typeName;
                })) {
                importedModules.insert(library.frustSourceModules.begin(), library.frustSourceModules.end());
                break;
            }
        }
    };

    // Shared by the pure-node pass below and by control-flow condition/
    // range/argument resolution -- same three sources an ordinary pure
    // node's inputs already use (a wired data node, a bound graph
    // parameter, or the pin's own default), with one added rule: a wired
    // data node must already be in boundDataNodes, or resolution fails.
    auto resolveInputExpr = [&](NodeId nodeId, const Pin& input, std::string& outExpr) -> bool {
        if (input.type.kind != PinKind::Data || FrustType(input.type.dataType).empty()) return false;
        if (const Connection* connection = InputConnection(graph, nodeId, input.id)) {
            const Node* source = graph.FindNode(connection->fromNode);
            const Pin* output = source ? source->FindPin(connection->fromPin) : nullptr;
            if (!output || output->type.kind != PinKind::Data || output->type.dataType != input.type.dataType) return false;
            if (!boundDataNodes.contains(connection->fromNode)) return false;
            outExpr = "n" + std::to_string(connection->fromNode);
            return true;
        }
        if (const auto bound = parameterBindings.find({ nodeId, input.id }); bound != parameterBindings.end()) {
            if (parameterTypes.at(bound->second) != input.type.dataType) return false;
            outExpr = bound->second;
            return true;
        }
        if (const std::string fallback = DefaultExpression(input); !fallback.empty()) {
            outExpr = fallback;
            return true;
        }
        return false;
    };

    // Deduped extern fn declarations for host-extern node types
    // (Node/Behavior Graph Foundations plan Phase 5 -- Variable get/set,
    // the entity-self accessor) actually referenced in this graph, keyed
    // by frustEntryPoint so the same host function used by several nodes
    // only gets declared once. Built alongside the pure-node loop below;
    // emitted into the header once the whole pass completes successfully.
    std::map<std::string, std::string> externDeclarationsByName;

    for (const NodeId id : *order) {
        const Node* node = graph.FindNode(id);
        const NodeTypeDescriptor* type = node ? libraries.FindNodeType(node->TypeName()) : nullptr;
        if (type != nullptr && type->domain == Domain::Event) continue;
        // Control-flow nodes (Branch/Sequence/For/While/Break/Continue/
        // Return) never reach here -- TopologicalDataOrder returns every
        // node in the graph regardless of exec structure, but these are
        // lowered by the structured exec walk below, not as pure
        // single-value data nodes. For's "index" data output is the one
        // exception with no home in either pass; see the For case below.
        if (type != nullptr && type->controlFlow != ControlFlowKind::None) continue;
        if (!node || !type || type->domain != Domain::Core || type->outputs.size() != 1 ||
            type->outputs.front().type.kind != PinKind::Data || type->frustEntryPoint.empty()) {
            result.error = "node " + std::to_string(id) + " is not a pure single-value FRust node";
            return result;
        }
        importNodeModules(*type);
        const std::string outputType = FrustType(type->outputs.front().type.dataType);
        if (outputType.empty()) {
            result.error = "node " + std::to_string(id) + " has an unsupported FRust output type";
            return result;
        }
        if (type->isHostExtern && !externDeclarationsByName.contains(type->frustEntryPoint)) {
            std::ostringstream decl;
            decl << "extern fn " << type->frustEntryPoint << "(";
            for (size_t index = 0; index < type->inputs.size(); ++index) {
                if (index != 0) decl << ", ";
                decl << type->inputs[index].name << ": " << FrustType(type->inputs[index].type.dataType);
            }
            decl << ") -> " << outputType << ";\n";
            externDeclarationsByName[type->frustEntryPoint] = decl.str();
        }
        body << "    let n" << id << ": " << outputType << " = " << type->frustEntryPoint << "(";
        for (size_t index = 0; index < node->Inputs().size(); ++index) {
            const Pin& input = node->Inputs()[index];
            std::string expr;
            if (!resolveInputExpr(id, input, expr)) {
                result.error = "node " + std::to_string(id) + " input '" + input.name +
                                "' is unconnected, has no default, or depends on a control-flow-scoped value "
                                "(not supported: wire that computation directly inside the control-flow node's own path)";
                return result;
            }
            if (index != 0) body << ", ";
            body << expr;
        }
        body << ");\n";
        boundDataNodes.insert(id);
    }

    // Structured exec lowering. Every control-flow node type registered in
    // core_control_flow.cpp gets real FRust here -- Branch to if/else,
    // Sequence to inlined sequential statements, For/While to FRust's own
    // for/while (which already re-evaluate correctly; see Codegen.h), Break/
    // Continue to bare statements, Return to an early exit. Deliberately
    // scoped: a Branch/Sequence's arms do not rejoin a shared continuation
    // in this version (each arm must reach its own end), and For's step
    // must be the literal default of 1 (FRust's `for i in a..b` has no step
    // parameter). Anything past that scope fails with a precise diagnostic,
    // per this project's own rule for unimplemented control-flow lowering --
    // never a silent wrong compile.
    int loopDepth = 0;
    std::function<bool(NodeId)> lowerExecChain = [&](NodeId start) -> bool {
        std::set<NodeId> visited;
        NodeId current = start;
        while (current != 0) {
            if (!visited.insert(current).second) {
                result.error = "execution cycle detected outside a loop construct";
                return false;
            }
            const Node* node = graph.FindNode(current);
            const NodeTypeDescriptor* type = node ? libraries.FindNodeType(node->TypeName()) : nullptr;
            if (!node || !type) {
                result.error = "execution reaches an unknown node " + std::to_string(current);
                return false;
            }

            // An Event node (On Tick/On Begin Play/On End Play -- Node/
            // Behavior Graph Foundations plan Phase 4) can only ever
            // legally be `start` itself: nothing wires exec INTO an
            // Event node, it IS where the chain starts. It has zero Exec
            // inputs and no frustEntryPoint of its own (it doesn't call
            // anything), so it doesn't fit the generic "callable node"
            // shape the ControlFlowKind::None case below handles -- it's
            // a pure marker, not a call. Skip straight to whatever's
            // wired to its "then" output, emitting nothing for the
            // marker itself. Found while running the plan's own
            // verification pass: "On Tick -> Return" -- about as minimal
            // a program as exists -- failed to compile before this fix.
            if (current == start && type->domain == Domain::Event) {
                const Connection* next = ExecThenConnection(graph, *node);
                current = next ? next->toNode : 0;
                continue;
            }

            switch (type->controlFlow) {
            case ControlFlowKind::None: {
                if (type->domain != Domain::Event || type->frustEntryPoint.empty()) {
                    result.error = "execution entry reaches a non-callable FRust node";
                    return false;
                }
                const auto execInputs = std::count_if(node->Inputs().begin(), node->Inputs().end(),
                                                        [](const Pin& pin) { return pin.type.kind == PinKind::Exec; });
                const auto execOutputs = std::count_if(node->Outputs().begin(), node->Outputs().end(),
                                                         [](const Pin& pin) { return pin.type.kind == PinKind::Exec; });
                if (execInputs != 1 || execOutputs != 1) {
                    result.error = "node " + std::to_string(current) + " requires structured branch or loop lowering";
                    return false;
                }
                importNodeModules(*type);
                body << "    " << type->frustEntryPoint << "(";
                bool first = true;
                for (const Pin& input : node->Inputs()) {
                    if (input.type.kind == PinKind::Exec) continue;
                    std::string expr;
                    if (!resolveInputExpr(current, input, expr)) {
                        result.error = "callable node " + std::to_string(current) + " input '" + input.name +
                                        "' is unconnected or depends on a control-flow-scoped value";
                        return false;
                    }
                    if (!first) body << ", ";
                    first = false;
                    body << expr;
                }
                body << ");\n";
                const Connection* next = ExecThenConnection(graph, *node);
                current = next ? next->toNode : 0;
                break;
            }
            case ControlFlowKind::Branch: {
                const Pin* condPin = InputPinNamed(*node, "condition");
                std::string condExpr;
                if (!condPin || !resolveInputExpr(current, *condPin, condExpr)) {
                    result.error = "branch node " + std::to_string(current) + " condition is unconnected or unsupported";
                    return false;
                }
                body << "    if (" << condExpr << ") {\n";
                if (const Connection* trueConn = ExecConnectionFromOutputNamed(graph, *node, "true"))
                    if (!lowerExecChain(trueConn->toNode)) return false;
                body << "    } else {\n";
                if (const Connection* falseConn = ExecConnectionFromOutputNamed(graph, *node, "false"))
                    if (!lowerExecChain(falseConn->toNode)) return false;
                body << "    };\n";
                current = 0;
                break;
            }
            case ControlFlowKind::Sequence: {
                for (const Pin& output : node->Outputs()) {
                    if (output.type.kind != PinKind::Exec) continue;
                    if (const Connection* conn = ExecConnectionFromOutputNamed(graph, *node, output.name.c_str()))
                        if (!lowerExecChain(conn->toNode)) return false;
                }
                current = 0;
                break;
            }
            case ControlFlowKind::For: {
                const Pin* firstPin = InputPinNamed(*node, "firstIndex");
                const Pin* lastPin = InputPinNamed(*node, "lastIndex");
                const Pin* stepPin = InputPinNamed(*node, "step");
                std::string firstExpr, lastExpr, stepExpr;
                if (!firstPin || !lastPin || !stepPin || !resolveInputExpr(current, *firstPin, firstExpr) ||
                    !resolveInputExpr(current, *lastPin, lastExpr) || !resolveInputExpr(current, *stepPin, stepExpr)) {
                    result.error = "for node " + std::to_string(current) + " range is unconnected or unsupported";
                    return false;
                }
                if (stepExpr != "1") {
                    result.error = "for node " + std::to_string(current) +
                                    ": FRust's for-loop only supports a fixed step of 1 (got '" + stepExpr + "')";
                    return false;
                }
                body << "    for n" << current << " in " << firstExpr << ".." << lastExpr << " {\n";
                boundDataNodes.insert(current); // "index" output -- the loop variable is literally n{current}.
                ++loopDepth;
                if (const Connection* bodyConn = ExecConnectionFromOutputNamed(graph, *node, "body"))
                    if (!lowerExecChain(bodyConn->toNode)) { --loopDepth; return false; }
                --loopDepth;
                body << "    };\n";
                const Connection* completed = ExecConnectionFromOutputNamed(graph, *node, "completed");
                current = completed ? completed->toNode : 0;
                break;
            }
            case ControlFlowKind::While: {
                const Pin* condPin = InputPinNamed(*node, "condition");
                std::string condExpr;
                if (!condPin || !resolveInputExpr(current, *condPin, condExpr)) {
                    result.error = "while node " + std::to_string(current) + " condition is unconnected or unsupported";
                    return false;
                }
                body << "    while (" << condExpr << ") {\n";
                ++loopDepth;
                if (const Connection* bodyConn = ExecConnectionFromOutputNamed(graph, *node, "body"))
                    if (!lowerExecChain(bodyConn->toNode)) { --loopDepth; return false; }
                --loopDepth;
                body << "    };\n";
                const Connection* completed = ExecConnectionFromOutputNamed(graph, *node, "completed");
                current = completed ? completed->toNode : 0;
                break;
            }
            case ControlFlowKind::Break:
                if (loopDepth == 0) { result.error = "break node " + std::to_string(current) + " is outside a loop"; return false; }
                body << "    break;\n";
                current = 0;
                break;
            case ControlFlowKind::Continue:
                if (loopDepth == 0) { result.error = "continue node " + std::to_string(current) + " is outside a loop"; return false; }
                body << "    continue;\n";
                current = 0;
                break;
            case ControlFlowKind::Return:
                // core.return carries no data pin of its own (see
                // core_control_flow.cpp) -- an early return exits with the
                // same result the graph's normal end already computes.
                if (!boundDataNodes.contains(options.resultNode)) {
                    result.error = "return node " + std::to_string(current) +
                                    " fires before the graph's result value is computed";
                    return false;
                }
                body << "    return n" << options.resultNode << ";\n";
                current = 0;
                break;
            }
        }
        return true;
    };

    if (options.entryNode != 0 && !lowerExecChain(options.entryNode)) {
        return result;
    }

    // No resultNode is legitimate now (an Event-triggered exec-only
    // chain) -- only validate an output pin when one was actually
    // requested.
    const Pin* outputPin = nullptr;
    if (options.resultNode != 0) {
        const Node* outputNode = graph.FindNode(options.resultNode);
        outputPin = outputNode ? outputNode->FindPin(options.resultPin) : nullptr;
        if (!outputPin || outputPin->isInput || outputPin->type.kind != PinKind::Data || FrustType(outputPin->type.dataType).empty()) {
            result.error = "the requested graph result must be a supported data output";
            return result;
        }
    }

    std::ostringstream source;
    if (options.emitManifestAndImports) {
        if (!options.manifestJson.empty()) source << "manifest \"" << EscapeString(options.manifestJson) << "\";\n\n";
        for (const auto& module : importedModules) {
            if (module.empty()) { result.error = "source module names cannot be empty"; return result; }
            source << "use self::" << module << ";\n";
        }
        if (!importedModules.empty()) source << '\n';
    }
    if (options.exposeAsNode) {
        // `node pure` for a pure data function (no entryNode -- nothing
        // but the return value), `node callable` when an entryNode gives
        // it exec/side-effect behavior. Either reflects into the node
        // registry the same way CoreNodesLibrary.frust's built-ins do.
        source << (options.entryNode != 0 ? "node callable " : "node pure ");
    }
    source << "pub fn " << options.functionName << "(";
    for (size_t index = 0; index < options.parameters.size(); ++index) {
        if (index != 0) source << ", ";
        source << options.parameters[index].name << ": " << FrustType(options.parameters[index].type);
    }
    // No result pin -> trivial i64 return, same shape core_trigger() already
    // uses for an exec-only callable with nothing meaningful to hand back.
    source << ") -> " << (outputPin ? FrustType(outputPin->type.dataType) : "i64") << " = {\n" << body.str()
           << "    " << (outputPin ? "n" + std::to_string(options.resultNode) : "0") << "\n}\n";
    for (const auto& [name, decl] : externDeclarationsByName) {
        (void)name;
        result.externDeclarations.push_back(decl);
    }
    result.ok = true;
    result.source = source.str();
    return result;
}

} // namespace ce::node_system
