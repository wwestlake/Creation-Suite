#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "node_system/graph.h"
#include "node_system/node.h"
#include "node_system/pin.h"

namespace ce::node_system {

enum class ControlFlowKind {
    None,
    Branch,
    Sequence,
    For,
    While,
    Break,
    Continue,
    Return,
};

enum class MonadOperation {
    None,
    Wrap,
    Bind,
    Map,
    Fail,
    Recover,
};

// One input or output pin's fixed shape, as promised by a registered
// node type -- name, type, and (for inputs) the STARTING default value
// AddRegisteredNode gives a freshly-constructed instance. A node
// instance is free to carry a DIFFERENT default afterward (see
// ValidateAgainstRegistry's own comment) -- that's the actual point of
// PinDefaultValue existing on a per-pin basis rather than being part of
// the registry-wide type shape: a Log node's "message" or a
// GetVariable's "name" (GS9's node catalog) is deliberately a
// per-instance literal, not a fixed constant every instance shares.
struct PinSignature {
    std::string name;
    PinTypeDesc type;
    PinDefaultValue defaultValue;

    // Non-empty when this pin's type is one of the node type's own
    // genericParams (see NodeTypeDescriptor::genericParams below) -- e.g.
    // "T" for a pin FRust reflection tagged with "genericParam":"T"
    // (Codegen.h's compileNodeReflection). `type.dataType` stays
    // DataType::Any for a generic pin, same as an ordinary untyped one;
    // this is what actually distinguishes "genuinely untyped" from
    // "generic, resolved per node instance" -- see ResolveEffectivePinType.
    std::string genericParam;
};

// GS8: the fix for Node::TypeName() being a completely free-form,
// unvalidated std::string today (nothing currently stops
// graph.AddNode("totally_made_up", Domain::Core) from producing a node
// with whatever pins the caller happens to add by hand, with no
// guarantee two nodes of the "same" type actually have the same
// shape). A NodeTypeDescriptor is the authoritative pin signature for
// one type name; AddRegisteredNode (below) is what actually constructs
// a node guaranteed to match it. Control-flow metadata identifies structured
// Suite nodes without coupling this graph layer to FRust syntax.
struct NodeTypeDescriptor {
    std::string typeName;
    Domain domain = Domain::Core;
    std::vector<PinSignature> inputs;
    std::vector<PinSignature> outputs;
    ControlFlowKind controlFlow = ControlFlowKind::None;
    MonadOperation monadOperation = MonadOperation::None;

    // Non-empty for a node type backed by a generic FRust `node pure`/
    // `node callable` function (e.g. `["T"]` for `fn identity<T>(x: T) ->
    // T`), straight from FRust reflection's own "genericParams" array
    // (Codegen.h's compileNodeReflection). Each placed instance of a
    // generic node type resolves every one of these to a concrete
    // DataType via Node::GenericBindings() before it can compile -- see
    // ResolveEffectivePinType.
    std::vector<std::string> genericParams;

    // Editor/documentation metadata and the FRust compilation contract,
    // ported from Creation Engine's own former NodeSystem fork as part of
    // unifying onto this one shared copy (docs/SHARED_EXTRACTION_PLAN.md,
    // "node-to-FRust shared contracts"; wwestlake/Creation-Suite#115).
    // frustEntryPoint/requiredCapabilities are metadata here, same as the
    // header comment above already established for controlFlow/
    // monadOperation -- CompileBehaviorGraphToFrust (frust_codegen.h) is
    // what actually interprets them; this struct just carries the values.
    std::string displayName;
    std::string category;
    std::string description;
    std::string frustEntryPoint;
    std::vector<std::string> requiredCapabilities;

    // True when frustEntryPoint names a host-registered `extern fn`
    // (Node/Behavior Graph Foundations plan Phase 5 -- Variable get/set,
    // the entity-self accessor) rather than a `use self::`-imported pure
    // FRust module function. CompileBehaviorGraphToFrust emits a
    // deduped `extern fn` declaration for each one actually referenced
    // in a compiled graph, built from this descriptor's own inputs/
    // outputs -- the host function's C++ signature must match exactly
    // (same convention every existing EngineFrustHost host function
    // already follows).
    bool isHostExtern = false;
};

class NodeTypeRegistry {
public:
    // Overwrites any existing registration under the same type name --
    // deliberately permissive (no "already registered" error) so a
    // future hot-reload of the node catalog itself can simply
    // re-register.
    void Register(NodeTypeDescriptor descriptor);

    // Removes one registered type by name, if present -- no-op otherwise.
    // Used when replacing a whole library's node set wholesale
    // (NodeLibraryRegistry::ReplaceLibrary) so a renamed/removed
    // dynamically-authored node type (e.g. a renamed Input Combo Event)
    // doesn't leave a stale entry other code could still resolve.
    void Unregister(const std::string& typeName);

    const NodeTypeDescriptor* Find(const std::string& typeName) const;
    const std::unordered_map<std::string, NodeTypeDescriptor>& Types() const { return types_; }

private:
    std::unordered_map<std::string, NodeTypeDescriptor> types_;
};

// Constructs a node in `graph` from `registry`'s descriptor for
// `typeName`, auto-populating every pin exactly as the descriptor
// specifies -- the actual mechanism that makes a registered type name
// mean something, instead of AddNode's existing free-form
// "typeName plus whatever pins the caller separately adds by hand."
// Returns nullptr (and fills `errorOut`, if given) if `typeName` isn't
// registered. Graph::AddNode itself is untouched and still available
// for ad hoc/dynamically-shaped nodes (e.g. a future CallFunction node
// whose pins depend on the target function's signature, not a fixed
// registry entry) -- this is an additive convenience, not a
// replacement.
Node* AddRegisteredNode(Graph& graph, const NodeTypeRegistry& registry, const std::string& typeName,
                         std::string* errorOut = nullptr);

// Checks every node in `graph` against `registry`: the type name must
// be registered, and the node's actual pins (name and type, in order)
// must match the descriptor's shape. Deliberately does NOT compare
// default values -- GS9's node catalog relies on nodes overriding a
// pin's default after construction (a Log node's literal message, a
// GetVariable's variable name, an ordinary Compare node's per-instance
// threshold), which is the entire reason PinDefaultValue lives on the
// pin instance rather than being baked into the registered shape.
// Nodes built via AddRegisteredNode always pass trivially (before any
// such override); this exists to catch actual SHAPE drift -- a node
// hand-built via the raw AddNode/AddInput/AddOutput API with the wrong
// pin count/name/type, or loaded from an older graph file saved before a
// type's registered pin shape changed. Returns true (with `errorsOut`
// left empty, if given) iff every node's shape matches.
bool ValidateAgainstRegistry(const Graph& graph, const NodeTypeRegistry& registry,
                              std::vector<std::string>* errorsOut = nullptr);

// The one authoritative place a pin's REAL type is determined, for any
// consumer that needs to know it -- codegen's `let` type annotations,
// turbofish argument order, and wiring-compatibility checks all go
// through this instead of reading `pin.type.dataType` directly. For an
// ordinary (non-generic) pin, that raw dataType IS the answer. For a pin
// whose matching PinSignature carries a `genericParam` name (found by
// name+direction in `type`'s inputs/outputs), the answer instead comes
// from `node`'s own per-instance `GenericBindings()` for that parameter
// name -- DataType::Any if unresolved (the caller's own "must be
// resolved before compiling" error path is expected to catch that
// separately; this function does not itself error). Without going
// through here, a downstream node reading from a generic node's OUTPUT
// pin would still see the descriptor-level Any even though this
// particular instance is concretely bound to e.g. Float.
DataType ResolveEffectivePinType(const NodeTypeDescriptor& type, const Node& node, const Pin& pin);

} // namespace ce::node_system
