#pragma once

#include <string>
#include <vector>

#include "node_system/node_library.h"

namespace ce::node_system {

// The behavior compiler deliberately emits ordinary FRust source.  It does
// not execute graph nodes itself: the generated root source imports the node
// library modules and is compiled by FRust as one plugin.
struct FrustGraphParameter {
    std::string name;
    DataType type = DataType::Int;
};

struct FrustGraphInputBinding {
    NodeId node = 0;
    PinId pin = 0;
    std::string parameterName;
};

struct FrustGraphCompileOptions {
    std::string functionName;
    std::vector<FrustGraphParameter> parameters;
    std::vector<FrustGraphInputBinding> inputBindings;
    NodeId resultNode = 0;
    PinId resultPin = 0;
    // Optional event entry. When supplied, callable nodes reached through
    // their `then` exec pins are emitted as ordered FRust calls before the
    // graph's data result is returned.
    NodeId entryNode = 0;
    // Names used in `use self::<name>;`.  The generated root must be saved
    // next to those .frust source modules before frust_plugin_load() runs.
    std::vector<std::string> sourceModules;
    std::string manifestJson;

    // When false, skips emitting the manifest/`use self::` header lines --
    // for compiling multiple functions (one per Event node, Node/Behavior
    // Graph Foundations plan Phase 4) into ONE .frust file: the caller
    // compiles the first with this true, the rest with this false, and
    // concatenates the results.
    bool emitManifestAndImports = true;

    // Orthogonal node-exposure capability (Pod Management System plan,
    // Phase 5): when set, the emitted function is prefixed `node pure`
    // (no entryNode -- pure data) or `node callable` (entryNode set)
    // instead of plain `pub fn`, so the compiler's own reflection picks
    // it up as a draggable node type in other graphs -- see
    // FRUST_NODE_LIBRARIES.md's single-file self-reflection note; no
    // manifest/sibling-module split is needed for this.
    bool exposeAsNode = false;
};

struct FrustGraphCompileResult {
    bool ok = false;
    std::string source;
    std::string error;

    // `extern fn` declarations (one per distinct host-extern node type
    // actually referenced) this compile needed -- Node/Behavior Graph
    // Foundations plan Phase 5. Returned as data, NOT embedded in
    // `source`, because a multi-function compile (one call per Event
    // node, same options.emitManifestAndImports convention) needs these
    // deduped ACROSS every call before any of them are emitted, not
    // once per call -- the same host function used by two different
    // compiled functions must only be declared once in the final file.
    std::vector<std::string> externDeclarations;
};

FrustGraphCompileResult CompileBehaviorGraphToFrust(const Graph& graph,
                                                     const NodeLibraryRegistry& libraries,
                                                     const FrustGraphCompileOptions& options);

// Backslash-escapes `\` and `"` for embedding in a FRust string literal
// (e.g. a `manifest "...";` line). Exposed publicly so a caller
// assembling its own multi-function file header (Node/Behavior Graph
// Foundations plan Phase 4/5 -- one manifest shared across several
// separately-compiled functions) escapes manifestJson exactly the way
// this compiler already does internally, instead of duplicating the
// escaping logic.
std::string EscapeFrustString(const std::string& value);

} // namespace ce::node_system
