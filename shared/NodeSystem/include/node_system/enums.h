#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "node_system/pin.h"

// Enums: a named list of choices for an integer setting - a Blend node's mode (Normal, Multiply, Screen...), a
// gradient's direction - so a setting shows meaningful labels instead of a bare number. Suite standard for every
// node system (shared/NodeSystem/SYMBOLS.md, "Enums"), following the research node language's schematic
// (FrustLang projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md, "enums": a name and its variants).
//
// The value stays an integer underneath - variant i is the value i - so wires, files and generated code carry a
// number. A pin is tagged with its enum through PinTypeDesc::enumType; node types register their enums in the
// NodeTypeRegistry next to the node types that use them.
//
// Enums are full sum types (TYPES.md, FRUST_LANG_SPEC.md 5.2): a variant may carry values - none, one or several,
// each of any type, including structs and enums (the enum itself too: a recursive type). The variant number still
// travels with the value, so such an enum still drives a Switch and reads as a number; where the carried values are
// wanted (Match, a struct member), they travel alongside it.
namespace ce::node_system {

// Where a type is defined, which is where it can be used (TYPES.md): built into an app, from a pod, the project's
// (every graph in the project, any app), or one graph's own. The closest scope wins a name.
enum class TypeScope { builtin, pod, project, graph };

// A value a variant carries. FRust's payloads are positional (`Solid(Array<f64, 3>, f64)`); the name is for people
// and for the pins that carry it (Make Variant, Match).
struct EnumField {
    std::string name;
    PinTypeDesc type;
    std::string description;

    bool operator==(const EnumField&) const = default;
};

struct EnumVariant {
    std::string name;          // shown to people; its FRust variant is FrustIdentifier(name)
    std::string description;
    std::uint32_t colour = 0;  // 0xAARRGGBB, 0 for none - colours the value wherever it is shown (a Switch's cases)
    std::vector<EnumField> fields; // what this variant carries, in order; empty for a plain value

    EnumVariant() = default;
    EnumVariant(std::string variantName) : name(std::move(variantName)) {}
    EnumVariant(const char* variantName) : name(variantName) {}
    bool operator==(const EnumVariant&) const = default;
};

struct EnumDef {
    std::string name;                  // identifier, no spaces, stable once made: "BlendMode" - what pins and Choices name
    std::string displayName;           // shown to people: "Blend Mode"
    std::vector<EnumVariant> variants; // value i is variants[i]: "Normal", "Multiply"...
    std::string description;
    TypeScope scope = TypeScope::builtin;

    bool operator==(const EnumDef&) const = default;
};

// A FRust identifier from a display name: words joined, each capitalised, anything else dropped ("HSV channel" ->
// "HsvChannel", "Deep Snow" -> "DeepSnow"). "Unnamed" if nothing is left; a leading digit gets an underscore.
std::string FrustIdentifier(const std::string& displayName, bool capitaliseFirst = true);

// A new enum name, unique in `existing`, from a display name ("HSV Channel" -> "HsvChannel", then "HsvChannel2").
std::string MakeEnumName(const std::string& displayName, const std::vector<EnumDef>& existing);

// The FRust declaration of an enum: "enum HsvChannel { Hue, Saturation, Value }", or with what variants carry:
// "enum Fill { Nothing, Solid(Array<f64, 3>, f64) }". Variant i is value i, as everywhere.
std::string FrustEnumDeclaration(const EnumDef& def);

// True if any variant carries values (a sum type in full, not just named numbers).
bool EnumCarriesValues(const EnumDef& def);

// Names a field cannot have: Make Variant and Match use them for their own pins.
bool EnumFieldNameReserved(const std::string& fieldName);

// The label for a value, or the number itself if it is out of range.
inline std::string EnumVariantName(const EnumDef& def, std::int64_t value) {
    if (value >= 0 && value < static_cast<std::int64_t>(def.variants.size()))
        return def.variants[static_cast<std::size_t>(value)].name;
    return std::to_string(value);
}

} // namespace ce::node_system
