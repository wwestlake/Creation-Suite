#include "creation/material/material_compiler.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <sstream>
#include <unordered_map>

namespace ce::material {
namespace {
namespace ns = node_system;

const ns::Pin* Input(const ns::Node& node, const std::string& name)
{
    const auto it = std::find_if(node.Inputs().begin(), node.Inputs().end(), [&](const ns::Pin& pin) { return pin.name == name; });
    return it == node.Inputs().end() ? nullptr : &*it;
}

const ns::Connection* Incoming(const ns::Graph& graph, ns::NodeId nodeId, ns::PinId pinId)
{
    const auto it = std::find_if(graph.Connections().begin(), graph.Connections().end(),
                                 [&](const ns::Connection& c) { return c.toNode == nodeId && c.toPin == pinId; });
    return it == graph.Connections().end() ? nullptr : &*it;
}

// GLSL float literals require a decimal point or exponent -- "0f" is a
// compile error, "0.0f" isn't. std::ostringstream's default formatting
// drops the point for whole numbers (0 -> "0"), which this codegen path
// never actually exercised on a real GPU compile until the material's
// sharing bug (see EngineSceneSerializer::restoreScene) was fixed --
// before that, no scene entity ever picked up a compiled material graph's
// source, so this always-latent bug never got compiled by the driver.
std::string FormatGlslFloat(float number)
{
    std::ostringstream out;
    out << number;
    std::string text = out.str();
    if (text.find('.') == std::string::npos && text.find('e') == std::string::npos && text.find('E') == std::string::npos)
        text += ".0";
    return text + "f";
}

std::string FloatLiteral(const ns::PinDefaultValue& value, float fallback)
{
    if (const auto* number = std::get_if<float>(&value)) return FormatGlslFloat(*number);
    return FormatGlslFloat(fallback);
}

std::string ColorLiteral(const ns::PinDefaultValue& value)
{
    if (const auto* color = std::get_if<ns::Vec3Default>(&value)) {
        return "vec3(" + FormatGlslFloat(color->x) + ", " + FormatGlslFloat(color->y) + ", " + FormatGlslFloat(color->z) + ")";
    }
    return "vec3(1.0f)";
}

std::string Sanitize(std::string name)
{
    std::replace_if(name.begin(), name.end(), [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_'); }, '_');
    if (name.empty() || std::isdigit(static_cast<unsigned char>(name.front()))) name.insert(name.begin(), '_');
    return name;
}

// Rotator (UE's canonical name) needs a helper -- GLSL has no local
// variable binding within a single expression, so inlining the rotation
// math would mean repeating the angle sub-expression four times. Emitted
// into declarations only when at least one Rotator node is actually used.
constexpr const char* kRotateUvFunction =
    "vec2 RotateUV(vec2 uv, float angle) {\n"
    "    vec2 c = uv - vec2(0.5f);\n"
    "    float s = sin(angle);\n"
    "    float co = cos(angle);\n"
    "    return vec2(c.x * co - c.y * s, c.x * s + c.y * co) + vec2(0.5f);\n"
    "}\n";

// Normal From Height: live surface-gradient bump mapping (Blinn 1978,
// "Simulation of Wrinkled Surfaces", generalized to unparametrized
// surfaces -- no baked texture, no tangent/bitangent basis needed).
// Builds a local basis from the screen-space derivatives of world
// position, projects the height signal's own screen-space derivatives
// onto it, and perturbs the surface's real normal by the result. Emitted
// only when a Normal From Height node is actually used.
constexpr const char* kNormalFromHeightFunction =
    "vec3 NormalFromHeight(vec3 N, vec3 worldPos, float height, float strength) {\n"
    "    vec3 dPdx = dFdx(worldPos);\n"
    "    vec3 dPdy = dFdy(worldPos);\n"
    "    float dHdx = dFdx(height);\n"
    "    float dHdy = dFdy(height);\n"
    "    vec3 r1 = cross(dPdy, N);\n"
    "    vec3 r2 = cross(N, dPdx);\n"
    "    float det = dot(dPdx, r1);\n"
    "    vec3 surfaceGrad = (r1 * dHdx + r2 * dHdy) / max(abs(det), 1e-6f);\n"
    "    return normalize(N - strength * surfaceGrad);\n"
    "}\n";

} // namespace

MaterialCompileResult CompileMaterialGraph(const ns::Graph& graph, const ns::NodeTypeRegistry& registry)
{
    MaterialCompileResult result;
    std::unordered_map<ns::NodeId, std::string> cache;
    std::unordered_map<ns::NodeId, bool> visiting;
    std::unordered_map<std::string, MaterialParameter> parameters;
    std::unordered_map<std::string, std::string> textureSlots; // path -> generated uniform name
    bool usesRotator = false;
    bool usesNormalFromHeight = false;

    // Unary/binary GLSL builtins that differ from their material node name
    // only by capitalization/spelling (UE's canonical names again) -- kept
    // as lookup tables rather than one branch per node to avoid ~15 nearly
    // identical if/else arms.
    static const std::unordered_map<std::string, std::string> kUnaryFloatFn = {
        {"material.abs", "abs"}, {"material.ceil", "ceil"}, {"material.floor", "floor"},
        {"material.frac", "fract"}, {"material.sqrt", "sqrt"}, {"material.sine", "sin"}, {"material.cosine", "cos"},
    };

    std::function<std::string(ns::NodeId, ns::PinId)> emit = [&](ns::NodeId nodeId, ns::PinId outputPin) -> std::string {
        const auto* node = graph.FindNode(nodeId);
        if (node == nullptr) { result.errors.push_back("Material graph references a missing node."); return "0.0f"; }
        if (visiting[nodeId]) { result.errors.push_back("Material graph contains a data cycle at node " + std::to_string(nodeId) + "."); return "0.0f"; }
        if (const auto cached = cache.find(nodeId); cached != cache.end()) return cached->second;
        visiting[nodeId] = true;
        std::string expression;
        const auto outputIt = std::find_if(node->Outputs().begin(), node->Outputs().end(), [&](const ns::Pin& pin) { return pin.id == outputPin; });
        const std::string outputName = outputIt == node->Outputs().end() ? "" : outputIt->name;
        auto inputExpression = [&](const char* name, const std::string& fallback) {
            const auto* pin = Input(*node, name);
            if (pin == nullptr) return fallback;
            if (const auto* connection = Incoming(graph, nodeId, pin->id)) return emit(connection->fromNode, connection->fromPin);
            return fallback;
        };
        const auto& type = node->TypeName();

        if (type == "material.uv0") expression = "vUV";
        else if (type == "material.worldposition") expression = "worldPosition";
        else if (type == "material.normal") expression = "worldNormal";
        else if (type == "material.cameravector") expression = "cameraVector";
        else if (type == "material.reflectionvector") expression = "reflect(-cameraVector, worldNormal)";
        else if (type == "material.time") expression = "time";
        else if (type == "material.constant.float") {
            const auto* valuePin = Input(*node, "value");
            expression = FloatLiteral(valuePin != nullptr ? valuePin->defaultValue : ns::PinDefaultValue{}, 0.0f);
        } else if (type == "material.constant.color") {
            const auto* valuePin = Input(*node, "value");
            expression = ColorLiteral(valuePin != nullptr ? valuePin->defaultValue : ns::PinDefaultValue{});
        } else if (type == "material.parameter.float" || type == "material.parameter.color") {
            const auto* namePin = Input(*node, "name");
            const auto* name = namePin != nullptr ? std::get_if<std::string>(&namePin->defaultValue) : nullptr;
            const std::string parameterName = Sanitize(name != nullptr ? *name : "Parameter");
            const auto* defaultPin = Input(*node, "default");
            MaterialParameter param;
            param.name = parameterName;
            if (type.ends_with(".color")) {
                param.type = ns::DataType::Color;
                param.defaultColor = ns::Vec3Default{1.0f, 1.0f, 1.0f};
                if (const auto* color = defaultPin != nullptr ? std::get_if<ns::Vec3Default>(&defaultPin->defaultValue) : nullptr)
                    param.defaultColor = *color;
            } else {
                param.type = ns::DataType::Float;
                if (const auto* number = defaultPin != nullptr ? std::get_if<float>(&defaultPin->defaultValue) : nullptr)
                    param.defaultFloat = *number;
            }
            parameters[parameterName] = param;
            expression = "uMaterial_" + parameterName;
        } else if (type == "material.texture.sample2d") {
            const auto* texturePin = Input(*node, "texture");
            const auto* path = texturePin != nullptr ? std::get_if<std::string>(&texturePin->defaultValue) : nullptr;
            if (path == nullptr || path->empty()) {
                result.errors.push_back("Texture Sample node has no texture file path set.");
            } else {
                auto& uniformName = textureSlots[*path];
                if (uniformName.empty()) uniformName = "uMaterialTex_" + Sanitize(std::to_string(textureSlots.size()) + "_" + *path);
                expression = "texture(" + uniformName + ", " + inputExpression("uv", "vUV") + ").rgb";
            }
        } else if (type == "material.surface.output") {
            result.errors.push_back("Material Output is not a value expression.");
        } else if (type == "material.power") {
            expression = "pow(" + inputExpression("base", "1.0f") + ", " + inputExpression("exponent", "1.0f") + ")";
        } else if (type == "material.fmod") {
            expression = "mod(" + inputExpression("a", "0.0f") + ", " + inputExpression("b", "1.0f") + ")";
        } else if (type == "material.clamp") {
            expression = "clamp(" + inputExpression("value", "0.0f") + ", " + inputExpression("min", "0.0f") + ", " + inputExpression("max", "1.0f") + ")";
        } else if (type == "material.lerp") {
            expression = "mix(" + inputExpression("a", "0.0f") + ", " + inputExpression("b", "1.0f") + ", " + inputExpression("alpha", "0.5f") + ")";
        } else if (type == "material.oneminus") {
            expression = "(1.0f - " + inputExpression("value", "0.0f") + ")";
        } else if (type == "material.if") {
            expression = "(" + inputExpression("a", "0.0f") + " > " + inputExpression("b", "0.0f") + " ? " + inputExpression("aGreaterThanB", "1.0f")
                       + " : (" + inputExpression("a", "0.0f") + " == " + inputExpression("b", "0.0f") + " ? " + inputExpression("aEqualsB", "0.0f")
                       + " : " + inputExpression("aLessThanB", "0.0f") + "))";
        } else if (type == "material.append") {
            expression = "vec3(" + inputExpression("x", "0.0f") + ", " + inputExpression("y", "0.0f") + ", " + inputExpression("z", "0.0f") + ")";
        } else if (type == "material.componentmask") {
            const auto* channelPin = Input(*node, "channel");
            const auto* channel = channelPin != nullptr ? std::get_if<std::string>(&channelPin->defaultValue) : nullptr;
            const std::string swizzle = (channel != nullptr && (*channel == "g" || *channel == "b")) ? *channel : "r";
            expression = "(" + inputExpression("value", "vec3(0.0f)") + ")." + swizzle;
        } else if (type == "material.dotproduct") {
            expression = "dot(" + inputExpression("a", "vec3(0.0f)") + ", " + inputExpression("b", "vec3(0.0f)") + ")";
        } else if (type == "material.crossproduct") {
            expression = "cross(" + inputExpression("a", "vec3(0.0f)") + ", " + inputExpression("b", "vec3(0.0f)") + ")";
        } else if (type == "material.normalize") {
            expression = "normalize(" + inputExpression("value", "vec3(1.0f, 0.0f, 0.0f)") + ")";
        } else if (type == "material.fresnel") {
            const std::string exponent = inputExpression("exponent", "5.0f");
            const std::string base = inputExpression("baseReflectFraction", "0.04f");
            expression = "(" + base + " + (1.0f - " + base + ") * pow(clamp(1.0f - dot(worldNormal, cameraVector), 0.0f, 1.0f), " + exponent + "))";
        } else if (type == "material.panner") {
            expression = "(" + inputExpression("coordinate", "vUV") + " + vec2(" + inputExpression("speedX", "0.0f") + ", "
                       + inputExpression("speedY", "0.0f") + ") * time)";
        } else if (type == "material.rotator") {
            usesRotator = true;
            expression = "RotateUV(" + inputExpression("coordinate", "vUV") + ", time * " + inputExpression("speed", "0.0f") + " * 6.283185307f)";
        } else if (type == "material.ddx") {
            expression = "dFdx(" + inputExpression("value", "0.0f") + ")";
        } else if (type == "material.ddy") {
            expression = "dFdy(" + inputExpression("value", "0.0f") + ")";
        } else if (type == "material.normalfromheight") {
            usesNormalFromHeight = true;
            expression = "NormalFromHeight(worldNormal, worldPosition, " + inputExpression("height", "0.0f") + ", "
                       + inputExpression("strength", "1.0f") + ")";
        } else if (const auto unary = kUnaryFloatFn.find(type); unary != kUnaryFloatFn.end()) {
            expression = unary->second + "(" + inputExpression("value", "0.0f") + ")";
        } else if (type.starts_with("material.") && type.ends_with(".float")) {
            const auto op = type.substr(9, type.size() - 15);
            const std::string symbol = op == "add" ? "+" : op == "multiply" ? "*" : op == "subtract" ? "-" : "/";
            expression = "(" + inputExpression("a", "0.0f") + " " + symbol + " " + inputExpression("b", "0.0f") + ")";
        } else {
            result.errors.push_back("Unsupported material node: " + type);
        }
        visiting[nodeId] = false;
        cache[nodeId] = expression;
        return expression;
    };

    const ns::Node* output = nullptr;
    for (const auto& [id, node] : graph.Nodes()) if (node->TypeName() == "material.surface.output") {
        if (output != nullptr) result.errors.push_back("Material graph must contain exactly one Material Output node.");
        output = node.get();
    }
    if (output == nullptr) result.errors.push_back("Material graph is missing a Material Output node.");
    if (!result.errors.empty()) return result;

    const auto value = [&](const char* name, const std::string& fallback) {
        const auto* pin = Input(*output, name);
        if (pin == nullptr) return fallback;
        if (const auto* connection = Incoming(graph, output->Id(), pin->id)) return emit(connection->fromNode, connection->fromPin);
        // Unconnected Normal means "use the surface's own normal as-is" --
        // its pin default (see material_nodes.cpp) is never meaningful,
        // unlike baseColor/metallic/roughness where the default IS the
        // intended unconnected value.
        if (name == std::string("normal")) return fallback;
        const bool isColorTyped = name == std::string("baseColor") || name == std::string("worldPositionOffset");
        return isColorTyped ? ColorLiteral(pin->defaultValue) : FloatLiteral(pin->defaultValue, name == std::string("roughness") ? 0.5f : 0.0f);
    };
    const std::string baseColor = value("baseColor", "vec3(0.8f)");
    const std::string metallic = value("metallic", "0.0f");
    const std::string roughness = value("roughness", "0.5f");
    const std::string normal = value("normal", "worldNormal");
    // No special-casing needed here the way "normal" needed one -- an
    // unconnected offset genuinely should be its own literal default
    // (vec3(0), see material_nodes.cpp), not a fallback to some existing
    // in-scope variable.
    const std::string worldPositionOffset = value("worldPositionOffset", "vec3(0.0f)");
    if (!result.errors.empty()) return result;

    if (usesRotator) result.source.declarations += kRotateUvFunction;
    if (usesNormalFromHeight) result.source.declarations += kNormalFromHeightFunction;

    std::ostringstream function;
    function << "void EvaluateMaterial(in vec2 vUV, in vec3 worldPosition, in vec3 worldNormal, in vec3 cameraVector, in float time,\n"
             << "                      out vec3 baseColor, out float metallic, out float roughness, out vec3 normal) {\n"
             << "    baseColor = " << baseColor << ";\n"
             << "    metallic = " << metallic << ";\n"
             << "    roughness = " << roughness << ";\n"
             << "    normal = " << normal << ";\n}";
    result.source.evaluateFunction = function.str();

    // Same parameter shape as EvaluateMaterial (worldPosition/worldNormal/
    // cameraVector/time are all genuinely valid per-vertex, not just
    // per-pixel -- only DDX/DDY have no vertex-stage equivalent), plus
    // localPosition (pre-model-transform, for offsets that want to stay
    // in object space) that EvaluateMaterial has no equivalent need for.
    std::ostringstream vertexFunction;
    vertexFunction << "vec3 EvaluateWorldPositionOffset(in vec3 localPosition, in vec3 worldPosition, in vec3 worldNormal,\n"
                   << "                                  in vec3 cameraVector, in float time, in vec2 vUV) {\n"
                   << "    return " << worldPositionOffset << ";\n}";
    result.source.vertexFunction = vertexFunction.str();

    for (const auto& [name, param] : parameters) {
        result.source.parameters.push_back(param);
        result.source.declarations += param.type == ns::DataType::Color ? "uniform vec3 uMaterial_" + name + ";\n" : "uniform float uMaterial_" + name + ";\n";
    }
    for (const auto& [path, uniformName] : textureSlots) {
        result.source.textures.push_back({path, uniformName});
        result.source.declarations += "uniform sampler2D " + uniformName + ";\n";
    }
    result.ok = true;
    return result;
}

} // namespace ce::material
