#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

// Just enough JSON for the ledger file: objects, arrays, strings, integers and booleans, with the standard escapes
// (including \uXXXX and surrogate pairs). Not a general library.
namespace creation::assistant::detail
{
struct JsonValue
{
    enum class Type { null, boolean, integer, string, array, object };

    Type type = Type::null;
    bool boolean = false;
    std::int64_t integer = 0;
    std::string string;
    std::vector<JsonValue> items;                            // array
    std::vector<std::pair<std::string, JsonValue>> members;  // object, in file order

    // The member called `name`, or nullptr.
    const JsonValue* find(const std::string& name) const
    {
        for (const auto& [key, value] : members)
            if (key == name)
                return &value;
        return nullptr;
    }
};

// Parses `text`. On failure returns false with a short reason in `error`.
bool parseJson(const std::string& text, JsonValue& out, std::string& error);

// `text` as a JSON string literal, quotes included.
std::string quoteJson(const std::string& text);
}
