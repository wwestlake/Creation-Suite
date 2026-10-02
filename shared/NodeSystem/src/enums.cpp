#include "node_system/enums.h"
#include "node_system/structs.h"

#include <algorithm>
#include <cctype>

namespace ce::node_system {

std::string FrustIdentifier(const std::string& displayName, bool capitaliseFirst) {
    // Words are runs of letters and digits. Each starts with a capital; the rest keeps its case, except a word that is
    // all capitals (an acronym, "HSV") which becomes "Hsv" - so "HsvChannel" stays as it is and "HSV channel" matches it.
    std::string result;
    std::string word;
    auto flush = [&result, &word, capitaliseFirst]() {
        if (word.empty())
            return;
        const bool acronym = word.size() > 1 && std::none_of(word.begin(), word.end(), [](char ch) {
            return std::islower(static_cast<unsigned char>(ch)) != 0;
        });
        for (size_t i = 0; i < word.size(); ++i) {
            const auto c = static_cast<unsigned char>(word[i]);
            if (i == 0)
                result += (result.empty() && !capitaliseFirst) ? static_cast<char>(c) : static_cast<char>(std::toupper(c));
            else
                result += acronym ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
        }
        word.clear();
    };
    for (char raw : displayName) {
        if (std::isalnum(static_cast<unsigned char>(raw)))
            word += raw;
        else
            flush();
    }
    flush();
    if (result.empty()) {
        return "Unnamed";
    }
    if (std::isdigit(static_cast<unsigned char>(result.front()))) {
        result.insert(result.begin(), '_');
    }
    return result;
}

std::string MakeEnumName(const std::string& displayName, const std::vector<EnumDef>& existing) {
    const std::string base = FrustIdentifier(displayName);
    auto taken = [&existing](const std::string& name) {
        return std::any_of(existing.begin(), existing.end(), [&name](const EnumDef& e) { return e.name == name; });
    };
    if (!taken(base)) {
        return base;
    }
    for (int n = 2;; ++n) {
        const std::string candidate = base + std::to_string(n);
        if (!taken(candidate)) {
            return candidate;
        }
    }
}

std::string FrustEnumDeclaration(const EnumDef& def) {
    std::string declaration = "enum " + FrustIdentifier(def.name) + " { ";
    for (size_t i = 0; i < def.variants.size(); ++i) {
        const auto& variant = def.variants[i];
        declaration += (i == 0 ? "" : ", ") + FrustIdentifier(variant.name);
        if (!variant.fields.empty()) {
            declaration += "(";
            for (size_t f = 0; f < variant.fields.size(); ++f) {
                declaration += (f == 0 ? "" : ", ") + FrustMemberType(variant.fields[f].type);
            }
            declaration += ")";
        }
    }
    return declaration + " }";
}

bool EnumCarriesValues(const EnumDef& def) {
    return std::any_of(def.variants.begin(), def.variants.end(), [](const EnumVariant& v) { return !v.fields.empty(); });
}

bool EnumFieldNameReserved(const std::string& fieldName) {
    return fieldName.empty() || fieldName == "type" || fieldName == "variant" || fieldName == "value";
}

} // namespace ce::node_system
