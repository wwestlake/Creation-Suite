#include "node_system/structs.h"

#include <algorithm>

namespace ce::node_system {

std::string MakeTypeName(const std::string& displayName, const std::vector<std::string>& taken) {
    const std::string base = FrustIdentifier(displayName);
    if (std::find(taken.begin(), taken.end(), base) == taken.end()) {
        return base;
    }
    for (int n = 2;; ++n) {
        const std::string candidate = base + std::to_string(n);
        if (std::find(taken.begin(), taken.end(), candidate) == taken.end()) {
            return candidate;
        }
    }
}

std::string FrustMemberType(const PinTypeDesc& type) {
    if (!type.enumType.empty()) {
        return FrustIdentifier(type.enumType);
    }
    switch (type.dataType) {
        case DataType::Float: return "f64";
        case DataType::Int: return "i64";
        case DataType::Bool: return "bool";
        case DataType::String: return "String";
        case DataType::Color:
        case DataType::Vec3: return "Array<f64, 3>";
        case DataType::Struct: return FrustIdentifier(type.structType);
        default: return "i64"; // images, drawings, brushes and other references travel as handles
    }
}

std::string FrustStructDeclaration(const StructDef& def) {
    std::string declaration = "struct " + FrustIdentifier(def.name) + " { ";
    for (size_t i = 0; i < def.members.size(); ++i) {
        declaration += (i == 0 ? "" : ", ") + FrustIdentifier(def.members[i].name, false) + ": " + FrustMemberType(def.members[i].type);
    }
    return declaration + " }";
}

} // namespace ce::node_system
