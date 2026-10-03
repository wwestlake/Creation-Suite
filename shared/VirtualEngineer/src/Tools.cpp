#include <creation/agent/Tools.h>

#include <cmath>

namespace creation::agent
{
namespace
{
juce::String typeOf(const juce::var& value)
{
    if (value.isVoid() || value.isUndefined())
        return "nothing";
    if (value.isBool())
        return "boolean";
    if (value.isInt() || value.isInt64())
        return "integer";
    if (value.isDouble())
        return static_cast<double>(value) == std::floor(static_cast<double>(value)) ? "integer" : "number";
    if (value.isString())
        return "string";
    if (value.isArray())
        return "array";
    if (value.isObject())
        return "object";
    return "unknown";
}

bool fitsType(const juce::String& wanted, const juce::String& actual)
{
    return wanted == actual || (wanted == "number" && actual == "integer");
}

juce::String check(const juce::var& schema, const juce::var& value, const juce::String& where)
{
    if (! schema.isObject())
        return {};

    if (const auto type = schema.getProperty("type", {}); ! type.isVoid())
    {
        juce::StringArray wanted;
        if (const auto* list = type.getArray())
            for (const auto& t : *list)
                wanted.add(t.toString());
        else
            wanted.add(type.toString());
        const auto actual = typeOf(value);
        bool fits = false;
        for (const auto& w : wanted)
            fits = fits || fitsType(w, actual);
        if (! fits)
            return where + " must be " + wanted.joinIntoString(" or ") + ", not " + actual + ".";
    }

    if (const auto* choices = schema.getProperty("enum", {}).getArray())
    {
        bool found = false;
        for (const auto& choice : *choices)
            found = found || choice == value;
        if (! found)
        {
            juce::StringArray names;
            for (const auto& choice : *choices)
                names.add(choice.toString());
            return where + " must be one of: " + names.joinIntoString(", ") + ".";
        }
    }

    if (value.isInt() || value.isInt64() || value.isDouble())
    {
        const auto number = static_cast<double>(value);
        if (schema.hasProperty("minimum") && number < static_cast<double>(schema.getProperty("minimum", {})))
            return where + " must be at least " + schema.getProperty("minimum", {}).toString() + ".";
        if (schema.hasProperty("maximum") && number > static_cast<double>(schema.getProperty("maximum", {})))
            return where + " must be at most " + schema.getProperty("maximum", {}).toString() + ".";
    }

    if (value.isString() && schema.hasProperty("maxLength")
        && value.toString().length() > static_cast<int>(schema.getProperty("maxLength", {})))
        return where + " must be at most " + schema.getProperty("maxLength", {}).toString() + " characters.";

    if (const auto* items = value.getArray(); items != nullptr && schema.hasProperty("items"))
        for (int i = 0; i < items->size(); ++i)
            if (auto problem = check(schema.getProperty("items", {}), items->getReference(i), where + "[" + juce::String(i) + "]");
                problem.isNotEmpty())
                return problem;

    if (const auto* object = value.getDynamicObject())
    {
        const auto properties = schema.getProperty("properties", {});
        if (const auto* required = schema.getProperty("required", {}).getArray())
            for (const auto& name : *required)
                if (! object->hasProperty(name.toString()))
                    return where + " needs \"" + name.toString() + "\".";
        for (const auto& property : object->getProperties())
        {
            const auto name = property.name.toString();
            const auto propertySchema = properties.getProperty(property.name, {});
            if (propertySchema.isVoid())
            {
                if (schema.hasProperty("additionalProperties") && ! static_cast<bool>(schema.getProperty("additionalProperties", {})))
                    return where + " has no \"" + name + "\".";
                continue;
            }
            if (auto problem = check(propertySchema, property.value, where == "arguments" ? "\"" + name + "\"" : where + "." + name);
                problem.isNotEmpty())
                return problem;
        }
    }
    return {};
}

bool validName(const juce::String& name)
{
    return name.isNotEmpty() && name.length() <= 60 && name.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789_.")
        && ! name.startsWithChar('.') && ! name.endsWithChar('.') && ! name.contains("..");
}
} // namespace

juce::String effectName(Effect effect)
{
    switch (effect)
    {
        case Effect::read: return "read";
        case Effect::write: return "write";
        case Effect::destructive: return "destructive";
        case Effect::external: return "external";
    }
    return "read";
}

ToolResult ToolResult::success(juce::var data, juce::StringArray changes)
{
    ToolResult result;
    result.data = std::move(data);
    result.changes = std::move(changes);
    return result;
}

ToolResult ToolResult::failure(const juce::String& code, const juce::String& message, const juce::String& hint)
{
    ToolResult result;
    result.ok = false;
    result.errorCode = code;
    result.message = message;
    result.hint = hint;
    return result;
}

juce::var ToolResult::toVar() const
{
    auto* body = new juce::DynamicObject();
    body->setProperty("ok", ok);
    if (ok)
    {
        if (! data.isVoid())
            body->setProperty("data", data);
        if (! changes.isEmpty())
        {
            juce::Array<juce::var> list;
            for (const auto& change : changes)
                list.add(change);
            body->setProperty("changes", juce::var(list));
        }
    }
    else
    {
        auto* error = new juce::DynamicObject();
        error->setProperty("code", errorCode);
        error->setProperty("message", message);
        if (hint.isNotEmpty())
            error->setProperty("hint", hint);
        body->setProperty("error", juce::var(error));
    }
    return juce::var(body);
}

juce::String validateArguments(const juce::var& schema, const juce::var& arguments)
{
    return check(schema, arguments, "arguments");
}

bool ToolRegistry::add(ToolDefinition definition, ToolHandler handler, juce::String& error)
{
    if (! validName(definition.name))
        error = "A tool name is lower case letters, digits, _ and dots (\"texture.graph.add_node\"): \"" + definition.name + "\".";
    else if (entries.count(definition.name) != 0)
        error = "There is already a tool named " + definition.name + ".";
    else if (definition.description.trim().isEmpty())
        error = definition.name + " needs a description: it is what the model reads.";
    else if (definition.parameters.getProperty("type", {}).toString() != "object")
        error = definition.name + "'s parameters must be an object schema.";
    else if (handler == nullptr)
        error = definition.name + " has no handler.";
    if (error.isNotEmpty())
        return false;
    const auto name = definition.name;
    entries[name] = { std::move(definition), std::move(handler) };
    return true;
}

void ToolRegistry::remove(const juce::String& name)
{
    entries.erase(name);
}

const ToolRegistry::Entry* ToolRegistry::find(const juce::String& name) const
{
    const auto found = entries.find(name);
    return found != entries.end() ? &found->second : nullptr;
}

std::vector<ToolDefinition> ToolRegistry::definitions() const
{
    std::vector<ToolDefinition> out;
    for (const auto& [name, entry] : entries)
        out.push_back(entry.definition);
    return out;
}

juce::String ToolRegistry::wireName(const juce::String& name)
{
    return name.replaceCharacter('.', '-');
}
} // namespace creation::agent
