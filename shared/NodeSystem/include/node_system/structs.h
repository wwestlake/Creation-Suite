#pragma once

#include <string>
#include <vector>

#include "node_system/enums.h"
#include "node_system/pin.h"

// Structs (TYPES.md): a named group of members, made in the Struct Editor - not by a node, not inside a graph - and
// from then on a type things can be wherever it is in scope: a param's type, a wire's, another struct's member. It
// compiles to a FRust struct. Make and Break nodes (struct_nodes.h) use a struct; they do not create one.
//
// A struct wire is DataType::Struct with PinTypeDesc::structType naming the struct; only the same struct connects.
// Member values on a graph's wires are the app's to carry (an evaluator's own struct value); the node system knows
// the shape.
namespace ce::node_system {

struct StructMember {
    std::string name;            // shown, and its FRust field (FrustIdentifier, first letter small)
    PinTypeDesc type;            // any data type; an Int with enumType is an enum member, a Struct with structType a nested one
    PinDefaultValue defaultValue; // for value types; images, drawings, brushes and nested structs have none
    std::string description;
    bool hasRange = false;       // numbers: the range Properties offers
    double minimum = 0.0;
    double maximum = 1.0;

    bool operator==(const StructMember&) const = default;
};

struct StructDef {
    std::string name;            // identifier, no spaces, stable once made - what wires and params name
    std::string displayName;
    std::vector<StructMember> members;
    std::string description;
    TypeScope scope = TypeScope::builtin;

    bool operator==(const StructDef&) const = default;
};

// A new type name unique among both enums and structs in scope ("Surface Settings" -> "SurfaceSettings", then 2...).
std::string MakeTypeName(const std::string& displayName, const std::vector<std::string>& taken);

// The FRust type of a member: f64, i64, bool, String, Array<f64, 3> for a colour or vector, the enum's or struct's
// name, i64 for an image / drawing / brush handle.
std::string FrustMemberType(const PinTypeDesc& type);
// "struct SurfaceSettings { strength: f64, tint: Array<f64, 3>, mode: BlendMode }"
std::string FrustStructDeclaration(const StructDef& def);

} // namespace ce::node_system
