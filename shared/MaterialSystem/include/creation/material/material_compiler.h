#pragma once

#include <string>
#include <vector>

#include <node_system/graph.h>
#include <node_system/type_registry.h>

namespace ce::material {

struct MaterialParameter {
    std::string name;
    node_system::DataType type = node_system::DataType::Float;
    // Read from the parameter node's own "default" input at compile time --
    // the value a Material starts with before anything (FRust, an editor
    // panel) drives it at runtime. Only the field matching `type` is
    // meaningful.
    float defaultFloat = 0.0f;
    node_system::Vec3Default defaultColor;
};

// One Texture Sample node's reference, as authored (an absolute file
// path -- no asset-picker/import step yet, see material_nodes.cpp's
// Texture Sample description) paired with the sampler uniform name the
// compiler generated for it. The caller (MaterialGraphPanel) resolves
// `path` into an actual GPU texture (AssetCatalog::GetOrLoadTexture) and
// binds it to `uniformName` on the Material -- the compiler itself never
// touches the filesystem or a GL context.
struct MaterialTextureSlot {
    std::string path;
    std::string uniformName;
};

struct MaterialShaderSource {
    // Declarations are inserted before both host shaders' main() --
    // parameter uniforms and helper functions (RotateUV, NormalFromHeight)
    // are equally harmless to declare in a stage that doesn't end up
    // calling them. The host owns bindings and lighting policy.
    std::string declarations;
    // EvaluateMaterial(...) -- called from the FRAGMENT host
    // (material_host.frag). Always emitted, defaulting to the fixed
    // baseColor/metallic/roughness/normal(=worldNormal) when nothing's
    // wired to a given Material Output input.
    std::string evaluateFunction;
    // EvaluateWorldPositionOffset(...) -- called from the VERTEX host
    // (material_host.vert), same "always emitted, defaults to no
    // displacement" shape. Kept as a genuinely separate function (not
    // folded into evaluateFunction) because it runs at a different shader
    // stage with a different, narrower set of valid nodes -- see DDX/DDY/
    // Normal From Height's own descriptions in material_nodes.cpp.
    std::string vertexFunction;
    std::vector<MaterialParameter> parameters;
    std::vector<MaterialTextureSlot> textures;
};

struct MaterialCompileResult {
    bool ok = false;
    MaterialShaderSource source;
    std::vector<std::string> errors;
};

MaterialCompileResult CompileMaterialGraph(const node_system::Graph& graph,
                                           const node_system::NodeTypeRegistry& registry);

} // namespace ce::material
