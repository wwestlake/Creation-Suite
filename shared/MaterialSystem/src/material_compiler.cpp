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

std::string FloatLiteral(const ns::PinDefaultValue& value, float fallback)
{
    if (const auto* number = std::get_if<float>(&value)) {
        std::ostringstream out;
        out << *number << 'f';
        return out.str();
    }
    return std::to_string(fallback) + "f";
}

std::string ColorLiteral(const ns::PinDefaultValue& value)
{
    if (const auto* color = std::get_if<ns::Vec3Default>(&value)) {
        std::ostringstream out;
        out << "vec3(" << color->x << "f, " << color->y << "f, " << color->z << "f)";
        return out.str();
    }
    return "vec3(1.0f)";
}

std::string Sanitize(std::string name)
{
    std::replace_if(name.begin(), name.end(), [](char c) { return !(std::isalnum(static_cast<unsigned char>(c)) || c == '_'); }, '_');
    if (name.empty() || std::isdigit(static_cast<unsigned char>(name.front()))) name.insert(name.begin(), '_');
    return name;
}

} // namespace

MaterialCompileResult CompileMaterialGraph(const ns::Graph& graph, const ns::NodeTypeRegistry& registry)
{
    MaterialCompileResult result;
    std::unordered_map<ns::NodeId, std::string> cache;
    std::unordered_map<ns::NodeId, bool> visiting;
    std::unordered_map<std::string, ns::DataType> parameters;
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
        if (node->TypeName() == "material.uv0") expression = "vUV";
        else if (node->TypeName() == "material.constant.float") expression = FloatLiteral(node->Outputs().front().defaultValue, 0.0f);
        else if (node->TypeName() == "material.constant.color") expression = ColorLiteral(node->Outputs().front().defaultValue);
        else if (node->TypeName() == "material.parameter.float" || node->TypeName() == "material.parameter.color") {
            const auto* namePin = Input(*node, "name");
            const auto* name = namePin != nullptr ? std::get_if<std::string>(&namePin->defaultValue) : nullptr;
            const std::string parameterName = Sanitize(name != nullptr ? *name : "Parameter");
            const auto type = node->TypeName().ends_with(".color") ? ns::DataType::Color : ns::DataType::Float;
            parameters.emplace(parameterName, type);
            expression = "uMaterial_" + parameterName;
        } else if (node->TypeName() == "material.texture.sample2d") {
            const auto* texturePin = Input(*node, "texture");
            const auto* texture = texturePin != nullptr ? std::get_if<std::string>(&texturePin->defaultValue) : nullptr;
            expression = "texture(" + Sanitize(texture != nullptr ? *texture : "uMaterialTexture") + ", " + inputExpression("uv", "vUV") + ").rgb";
        } else if (node->TypeName() == "material.surface.output") {
            result.errors.push_back("Surface Output is not a value expression.");
        } else if (node->TypeName().starts_with("material.") && node->TypeName().ends_with(".float")) {
            const auto op = node->TypeName().substr(9, node->TypeName().size() - 15);
            const std::string symbol = op == "add" ? "+" : op == "multiply" ? "*" : op == "subtract" ? "-" : "/";
            expression = "(" + inputExpression("a", "0.0f") + " " + symbol + " " + inputExpression("b", "0.0f") + ")";
        } else {
            result.errors.push_back("Unsupported material node: " + node->TypeName());
        }
        visiting[nodeId] = false;
        cache[nodeId] = expression;
        return expression;
    };

    const ns::Node* output = nullptr;
    for (const auto& [id, node] : graph.Nodes()) if (node->TypeName() == "material.surface.output") {
        if (output != nullptr) result.errors.push_back("Material graph must contain exactly one Surface Output node.");
        output = node.get();
    }
    if (output == nullptr) result.errors.push_back("Material graph is missing a Surface Output node.");
    if (!result.errors.empty()) return result;

    const auto value = [&](const char* name, const std::string& fallback) {
        const auto* pin = Input(*output, name);
        if (pin == nullptr) return fallback;
        if (const auto* connection = Incoming(graph, output->Id(), pin->id)) return emit(connection->fromNode, connection->fromPin);
        return name == std::string("baseColor") ? ColorLiteral(pin->defaultValue) : FloatLiteral(pin->defaultValue, name == std::string("roughness") ? 0.5f : 0.0f);
    };
    std::ostringstream function;
    function << "void EvaluateMaterial(in vec2 vUV, out vec3 baseColor, out float metallic, out float roughness) {\n"
             << "    baseColor = " << value("baseColor", "vec3(0.8f)") << ";\n"
             << "    metallic = " << value("metallic", "0.0f") << ";\n"
             << "    roughness = " << value("roughness", "0.5f") << ";\n}";
    result.source.evaluateFunction = function.str();
    for (const auto& [name, type] : parameters) {
        result.source.parameters.push_back({name, type});
        result.source.declarations += type == ns::DataType::Color ? "uniform vec3 uMaterial_" + name + ";\n" : "uniform float uMaterial_" + name + ";\n";
    }
    result.ok = true;
    return result;
}

} // namespace ce::material
