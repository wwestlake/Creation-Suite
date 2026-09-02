#include "creation/material/material_nodes.h"

#include <tuple>
#include <utility>

namespace ce::material {
namespace ns = node_system;

// Node set and canonical names are patterned directly after Unreal
// Engine's Material Expression Reference (dev.epicgames.com,
// "Unreal Engine Material Expressions Reference") -- typeName stays a
// short internal identifier (compiler dispatch, .frgraph serialization);
// displayName is what the palette and node canvas actually show, and it's
// UE's own established vocabulary, not invented here. Vector-valued
// outputs (position, normal, camera/reflection vectors, cross product,
// normalize, append) all use DataType::Color rather than a separate Vec3
// -- both are backed by the identical Vec3Default (pin.h), and
// IsConnectionCompatible requires an exact DataType match, so splitting
// them would just wall off the extremely common "wire a normal into
// BaseColor to debug it" move for no real benefit; UE's own pin
// compatibility doesn't distinguish them either.
void RegisterMaterialNodes(ns::NodeTypeRegistry& registry)
{
    // --- Coordinates ---
    registry.Register({.typeName = "material.uv0", .domain = ns::Domain::Material,
                       .outputs = {{"uv", {ns::PinKind::Data, ns::DataType::Vec2}, {}}},
                       .displayName = "Texture Coordinate", .category = "Coordinates"});
    registry.Register({.typeName = "material.worldposition", .domain = ns::Domain::Material,
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "World Position", .category = "Coordinates"});
    registry.Register({.typeName = "material.panner", .domain = ns::Domain::Material,
                       .inputs = {{"coordinate", {ns::PinKind::Data, ns::DataType::Vec2}, {}},
                                  {"speedX", {ns::PinKind::Data, ns::DataType::Float}, 0.1f},
                                  {"speedY", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Vec2}, {}}},
                       .displayName = "Panner", .category = "Coordinates",
                       .description = "Scrolls UV coordinates over time."});
    registry.Register({.typeName = "material.rotator", .domain = ns::Domain::Material,
                       .inputs = {{"coordinate", {ns::PinKind::Data, ns::DataType::Vec2}, {}},
                                  {"speed", {ns::PinKind::Data, ns::DataType::Float}, 0.25f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Vec2}, {}}},
                       .displayName = "Rotator", .category = "Coordinates",
                       .description = "Rotates UV coordinates around their center over time."});

    // --- Vectors ---
    registry.Register({.typeName = "material.normal", .domain = ns::Domain::Material,
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Pixel Normal WS", .category = "Vectors"});
    registry.Register({.typeName = "material.cameravector", .domain = ns::Domain::Material,
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Camera Vector WS", .category = "Vectors",
                       .description = "Unit vector from the surface toward the camera."});
    registry.Register({.typeName = "material.reflectionvector", .domain = ns::Domain::Material,
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Reflection Vector WS", .category = "Vectors"});

    // --- Constants ---
    registry.Register({.typeName = "material.constant.float", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Constant", .category = "Constants"});
    registry.Register({.typeName = "material.constant.color", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{1.0f, 1.0f, 1.0f}}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Constant3Vector", .category = "Constants"});
    registry.Register({.typeName = "material.time", .domain = ns::Domain::Material,
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Time", .category = "Constants",
                       .description = "Seconds elapsed since the viewport's render context was created."});

    // --- Parameters ---
    registry.Register({.typeName = "material.parameter.float", .domain = ns::Domain::Material,
                       .inputs = {{"name", {ns::PinKind::Data, ns::DataType::String}, std::string("Parameter")},
                                  {"default", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Scalar Parameter", .category = "Parameters"});
    registry.Register({.typeName = "material.parameter.color", .domain = ns::Domain::Material,
                       .inputs = {{"name", {ns::PinKind::Data, ns::DataType::String}, std::string("Color")},
                                  {"default", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{1.0f, 1.0f, 1.0f}}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Vector Parameter", .category = "Parameters"});

    // --- Math ---
    for (const auto& [typeSuffix, display, symbol] : {
             std::tuple{"add", "Add", "+"}, std::tuple{"subtract", "Subtract", "-"},
             std::tuple{"multiply", "Multiply", "*"}, std::tuple{"divide", "Divide", "/"}}) {
        registry.Register({.typeName = std::string("material.") + typeSuffix + ".float", .domain = ns::Domain::Material,
                           .inputs = {{"a", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                      {"b", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                           .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                           .displayName = display, .category = "Math"});
    }
    for (const auto& [typeSuffix, display] : {
             std::pair{"abs", "Abs"}, std::pair{"ceil", "Ceil"}, std::pair{"floor", "Floor"},
             std::pair{"frac", "Frac"}, std::pair{"sqrt", "SquareRoot"}, std::pair{"sine", "Sine"},
             std::pair{"cosine", "Cosine"}, std::pair{"oneminus", "OneMinus"}}) {
        registry.Register({.typeName = std::string("material.") + typeSuffix, .domain = ns::Domain::Material,
                           .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                           .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                           .displayName = display, .category = "Math"});
    }
    registry.Register({.typeName = "material.power", .domain = ns::Domain::Material,
                       .inputs = {{"base", {ns::PinKind::Data, ns::DataType::Float}, 1.0f},
                                  {"exponent", {ns::PinKind::Data, ns::DataType::Float}, 1.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Power", .category = "Math"});
    registry.Register({.typeName = "material.fmod", .domain = ns::Domain::Material,
                       .inputs = {{"a", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"b", {ns::PinKind::Data, ns::DataType::Float}, 1.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Fmod", .category = "Math"});
    registry.Register({.typeName = "material.clamp", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"min", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"max", {ns::PinKind::Data, ns::DataType::Float}, 1.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Clamp", .category = "Math"});
    registry.Register({.typeName = "material.lerp", .domain = ns::Domain::Material,
                       .inputs = {{"a", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"b", {ns::PinKind::Data, ns::DataType::Float}, 1.0f},
                                  {"alpha", {ns::PinKind::Data, ns::DataType::Float}, 0.5f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "LinearInterpolate", .category = "Math"});
    registry.Register({.typeName = "material.if", .domain = ns::Domain::Material,
                       .inputs = {{"a", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"b", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"aGreaterThanB", {ns::PinKind::Data, ns::DataType::Float}, 1.0f},
                                  {"aEqualsB", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"aLessThanB", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "If", .category = "Math"});

    // --- Vector Ops ---
    registry.Register({.typeName = "material.append", .domain = ns::Domain::Material,
                       .inputs = {{"x", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"y", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"z", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "AppendVector", .category = "Vector Ops"});
    registry.Register({.typeName = "material.componentmask", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{}},
                                  {"channel", {ns::PinKind::Data, ns::DataType::String}, std::string("r")}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "ComponentMask", .category = "Vector Ops",
                       .description = "channel is \"r\", \"g\", or \"b\"."});
    registry.Register({.typeName = "material.dotproduct", .domain = ns::Domain::Material,
                       .inputs = {{"a", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{}},
                                  {"b", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{}}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "DotProduct", .category = "Vector Ops"});
    registry.Register({.typeName = "material.crossproduct", .domain = ns::Domain::Material,
                       .inputs = {{"a", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{}},
                                  {"b", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{}}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "CrossProduct", .category = "Vector Ops"});
    registry.Register({.typeName = "material.normalize", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{1.0f, 0.0f, 0.0f}}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Normalize", .category = "Vector Ops"});

    // --- Utility ---
    registry.Register({.typeName = "material.fresnel", .domain = ns::Domain::Material,
                       .inputs = {{"exponent", {ns::PinKind::Data, ns::DataType::Float}, 5.0f},
                                  {"baseReflectFraction", {ns::PinKind::Data, ns::DataType::Float}, 0.04f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "Fresnel", .category = "Utility",
                       .description = "View-angle-dependent edge term, using the surface's own normal and camera vector."});
    registry.Register({.typeName = "material.ddx", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "DDX", .category = "Utility",
                       .description = "Screen-space partial derivative in the X direction. Fragment-stage "
                                      "only (baseColor/metallic/roughness/normal) -- dFdx has no vertex-shader "
                                      "equivalent, so wiring this into World Position Offset won't compile."});
    registry.Register({.typeName = "material.ddy", .domain = ns::Domain::Material,
                       .inputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, 0.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Float}, {}}},
                       .displayName = "DDY", .category = "Utility",
                       .description = "Screen-space partial derivative in the Y direction. Fragment-stage "
                                      "only, same reason as DDX."});
    registry.Register({.typeName = "material.normalfromheight", .domain = ns::Domain::Material,
                       .inputs = {{"height", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"strength", {ns::PinKind::Data, ns::DataType::Float}, 1.0f}},
                       .outputs = {{"value", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Normal From Height", .category = "Utility",
                       .description = "Live surface-gradient bump mapping (Blinn 1978) from a height/luminance "
                                      "signal -- e.g. a Texture Sample's red channel via ComponentMask. No "
                                      "baking, no tangent basis needed: perturbs the surface's own world normal "
                                      "using screen-space derivatives of height and world position. "
                                      "Fragment-stage only (uses DDX/DDY internally) -- valid on Material "
                                      "Output's Normal input, not on World Position Offset."});

    // --- Texture ---
    registry.Register({.typeName = "material.texture.sample2d", .domain = ns::Domain::Material,
                       .inputs = {{"texture", {ns::PinKind::Data, ns::DataType::Texture}, std::string("")},
                                  {"uv", {ns::PinKind::Data, ns::DataType::Vec2}, {}}},
                       .outputs = {{"color", {ns::PinKind::Data, ns::DataType::Color}, {}}},
                       .displayName = "Texture Sample", .category = "Texture",
                       .description = "texture is an absolute file path (e.g. C:/art/rock.png), resolved into a "
                                      "real GPU texture when the graph compiles -- no asset picker or "
                                      "drag-and-drop yet, type the path directly."});

    // --- Output ---
    registry.Register({.typeName = "material.surface.output", .domain = ns::Domain::Material,
                       .inputs = {{"baseColor", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{0.8f, 0.8f, 0.8f}},
                                  {"metallic", {ns::PinKind::Data, ns::DataType::Float}, 0.0f},
                                  {"roughness", {ns::PinKind::Data, ns::DataType::Float}, 0.5f},
                                  // Unconnected means "use the surface's own
                                  // interpolated normal unmodified" -- see
                                  // material_compiler.cpp's special-cased
                                  // fallback for this pin specifically; the
                                  // Vec3Default here is never actually read.
                                  {"normal", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{0.0f, 0.0f, 1.0f}},
                                  // Evaluated in the VERTEX stage, not the
                                  // fragment stage every other input here
                                  // is -- see EvaluateWorldPositionOffset
                                  // in material_compiler.cpp. Unconnected
                                  // means no displacement (vec3(0)), the
                                  // one input here where that's actually
                                  // the pin's own literal default, not a
                                  // special-cased fallback.
                                  {"worldPositionOffset", {ns::PinKind::Data, ns::DataType::Color}, ns::Vec3Default{0.0f, 0.0f, 0.0f}}},
                       .displayName = "Material Output", .category = "Output"});
}

} // namespace ce::material
