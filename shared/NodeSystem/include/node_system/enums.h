#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Enums: a named list of choices for an integer setting - a Blend node's mode (Normal, Multiply, Screen...), a
// gradient's direction - so a setting shows meaningful labels instead of a bare number. Suite standard for every
// node system (shared/NodeSystem/SYMBOLS.md, "Enums"), following the research node language's schematic
// (FrustLang projects/10_node_compiler/NODE_SCHEMATIC_SCHEMA_V2.md, "enums": a name and its variants).
//
// The value stays an integer underneath - variant i is the value i - so wires, files and generated code carry a
// number. A pin is tagged with its enum through PinTypeDesc::enumType; node types register their enums in the
// NodeTypeRegistry next to the node types that use them.
namespace ce::node_system {

struct EnumDef {
    std::string name;                  // identifier, no spaces: "BlendMode"
    std::string displayName;           // shown to people: "Blend Mode"
    std::vector<std::string> variants; // value i is variants[i]: "Normal", "Multiply"...
    std::string description;

    bool operator==(const EnumDef&) const = default;
};

// The label for a value, or the number itself if it is out of range.
inline std::string EnumVariantName(const EnumDef& def, std::int64_t value) {
    if (value >= 0 && value < static_cast<std::int64_t>(def.variants.size()))
        return def.variants[static_cast<std::size_t>(value)];
    return std::to_string(value);
}

} // namespace ce::node_system
