#include "node_system/symbols.h"

#include <algorithm>
#include <cctype>

namespace ce::node_system {

std::string SymbolKindToString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Param: return "param";
        case SymbolKind::Constant: return "constant";
        case SymbolKind::Variable: return "variable";
    }
    return "param";
}

std::optional<SymbolKind> SymbolKindFromString(const std::string& text) {
    if (text == "param") return SymbolKind::Param;
    if (text == "constant") return SymbolKind::Constant;
    if (text == "variable") return SymbolKind::Variable;
    return std::nullopt;
}

const std::vector<std::string>& SymbolAccessibilities() {
    static const std::vector<std::string> values { "private", "graph", "module", "project", "public", "agent", "readonly", "hidden" };
    return values;
}

PinDefaultValue DefaultValueFor(DataType type) {
    switch (type) {
        case DataType::Float: return 0.0f;
        case DataType::Int: return std::int64_t { 0 };
        case DataType::Bool: return false;
        case DataType::String:
        case DataType::Texture: return std::string();
        case DataType::Vec3:
        case DataType::Color: return Vec3Default {};
        default: return {};
    }
}

std::string MakeSymbolId(const std::string& name, const std::vector<Symbol>& existing) {
    std::string base;
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            base += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        else if (!base.empty() && base.back() != '_')
            base += '_';
    }
    while (!base.empty() && base.back() == '_')
        base.pop_back();
    if (base.empty() || std::isdigit(static_cast<unsigned char>(base.front())))
        base = "symbol" + (base.empty() ? std::string() : "_" + base);

    auto taken = [&existing](const std::string& id) {
        return std::any_of(existing.begin(), existing.end(), [&id](const Symbol& s) { return s.id == id; });
    };
    std::string id = base;
    for (int n = 2; taken(id); ++n)
        id = base + "_" + std::to_string(n);
    return id;
}

} // namespace ce::node_system
