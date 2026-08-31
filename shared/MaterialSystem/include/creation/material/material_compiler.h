#pragma once

#include <string>
#include <vector>

#include <node_system/graph.h>
#include <node_system/type_registry.h>

namespace ce::material {

struct MaterialParameter {
    std::string name;
    node_system::DataType type = node_system::DataType::Float;
};

struct MaterialShaderSource {
    // Declarations are intended to be inserted before the host fragment
    // shader's main function. The host owns bindings and lighting policy.
    std::string declarations;
    // A complete, deterministic function the host can call from main().
    std::string evaluateFunction;
    std::vector<MaterialParameter> parameters;
};

struct MaterialCompileResult {
    bool ok = false;
    MaterialShaderSource source;
    std::vector<std::string> errors;
};

MaterialCompileResult CompileMaterialGraph(const node_system::Graph& graph,
                                           const node_system::NodeTypeRegistry& registry);

} // namespace ce::material
