#include "creation/material/material_nodes.h"

namespace ce::material {
namespace ns = node_system;

void RegisterMaterialNodes(ns::NodeTypeRegistry& registry)
{
    registry.Register({"material.uv0", ns::Domain::Material, {},
                       {{"uv", {ns::PinKind::Data, ns::DataType::Vec2}, {}}}});
    registry.Register({"material.constant.float", ns::Domain::Material, {},
                       {{"value", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}}});
    registry.Register({"material.constant.color", ns::Domain::Material, {},
                       {{"value", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{1.0f, 1.0f, 1.0f}}}});
    registry.Register({"material.parameter.float", ns::Domain::Material,
                       {{"name", {ns::PinKind::Data, ns::DataType::String}, std::string("Parameter")},
                        {"default", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}}});
    registry.Register({"material.parameter.color", ns::Domain::Material,
                       {{"name", {ns::PinKind::Data, ns::DataType::String}, std::string("Color")},
                        {"default", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{1.0f, 1.0f, 1.0f}}},
                       {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}}});
    for (const auto& typeName : {"add", "multiply", "subtract", "divide"}) {
        registry.Register({std::string("material.") + typeName + ".float", ns::Domain::Material,
                           {{"a", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                            {"b", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                           {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}}});
    }
    registry.Register({"material.texture.sample2d", ns::Domain::Material,
                       {{"texture", {ns::PinKind::Data, ns::DataType::Texture}, std::string("uMaterialTexture")},
                        {"uv", {ns::PinKind::Data, ns::DataType::Vec2}, {}}},
                       {{"color", {ns::PinKind::Data, ns::DataType::Color}, {}}}});
    registry.Register({"material.surface.output", ns::Domain::Material,
                       {{"baseColor", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{0.8f, 0.8f, 0.8f}},
                        {"metallic", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                        {"roughness", {ns::PinKind::Data, ns::DataType::Float}, 0.5f}},
                       {}});
}

} // namespace ce::material
