#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <map>
#include <vector>

// What an app lets its Virtual Engineer do (docs/architecture/Suite-Agent-Runtime-Spec.md, section 5): named, typed
// operations, each declared once with what it does to the user's work, and run by the app on its message thread.
namespace creation::agent
{
// What a tool does to the user's work. Declared by the tool's author and enforced by the engineer - never the model's
// opinion of how risky a call is.
enum class Effect
{
    read,        // looks only
    write,       // changes the open work, undone with the request (the engineer's snapshot covers it)
    destructive, // removes or overwrites the user's work: asks the user first, every time
    external,    // leaves the open work (saves, exports, network, money): asks the user first, every time
};

juce::String effectName(Effect effect);

// The structured result every tool returns (section 5.3): data, or an error the model can act on.
struct ToolResult
{
    bool ok = true;
    juce::var data;
    juce::StringArray changes; // what changed, in words (the action log)
    juce::String errorCode;    // "unknown_node", "wrong_type"...
    juce::String message;      // plain words
    juce::String hint;         // the next useful step

    static ToolResult success(juce::var data, juce::StringArray changes = {});
    static ToolResult failure(const juce::String& code, const juce::String& message, const juce::String& hint = {});
    juce::var toVar() const;
};

struct ToolDefinition
{
    juce::String name;        // "<app>.<area>.<verb>", lower case: letters, digits, _ and dots
    juce::String title;       // for people: "Add a node"
    juce::String description; // for the model: what it does, when to use it, what it returns
    juce::var parameters;     // JSON Schema for the arguments (an object); see validateArguments
    Effect effect = Effect::read;
};

// Runs one call on the message thread, with arguments already checked against the definition. `done` may be called at
// once or later (a tool that waits for the app, such as one that waits for an evaluation to finish).
using ToolHandler = std::function<void(const juce::var& arguments, std::function<void(ToolResult)> done)>;

// Problems with arguments against a JSON Schema, in words the model can correct: empty when they fit. The schema
// subset tools use: type (one or a list of object, string, integer, number, boolean, array), properties, required,
// additionalProperties false, enum, minimum, maximum, maxLength, items.
juce::String validateArguments(const juce::var& schema, const juce::var& arguments);

// Captures and restores one kind of state the tools change (section 8), so a whole request undoes at once.
struct StateDomain
{
    juce::String name;
    std::function<juce::var()> capture;
    std::function<void(const juce::var&)> restore;
};

class ToolRegistry final
{
public:
    // Adds a tool; false with the reason when its definition is not usable (bad name, a name in use, parameters that
    // are not an object schema).
    bool add(ToolDefinition definition, ToolHandler handler, juce::String& error);
    void remove(const juce::String& name);

    struct Entry
    {
        ToolDefinition definition;
        ToolHandler handler;
    };
    const Entry* find(const juce::String& name) const;
    std::vector<ToolDefinition> definitions() const;
    bool isEmpty() const noexcept { return entries.empty(); }

    // The name a provider is sent: providers allow letters, digits, _ and -, not dots.
    static juce::String wireName(const juce::String& name);

private:
    std::map<juce::String, Entry> entries;
};
} // namespace creation::agent
