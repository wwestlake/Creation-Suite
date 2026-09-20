#include "creation/ai/ToolRegistry.h"

#include <juce_core/juce_core.h>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <thread>

namespace creation::ai
{
namespace
{
bool isIntegerValue(const juce::var& v)
{
    if (v.isInt() || v.isInt64())
        return true;
    if (v.isDouble())
    {
        const double d = (double) v;
        return std::floor(d) == d;
    }
    return false;
}

bool isNumberValue(const juce::var& v)
{
    return v.isInt() || v.isInt64() || v.isDouble();
}

std::string describeType(const juce::var& v)
{
    if (v.isString()) return "a string";
    if (v.isBool()) return "a boolean";
    if (v.isInt() || v.isInt64()) return "an integer";
    if (v.isDouble()) return "a number";
    if (v.isArray()) return "an array";
    if (v.getDynamicObject() != nullptr) return "an object";
    return "null";
}

// Returns "" when `value` fits `schema`, else what is wrong, naming `path` (e.g. "code" or "points[2].time").
std::string validateValue(const juce::var& value, const juce::var& schema, const std::string& path)
{
    const auto where = path.empty() ? std::string("The arguments") : "'" + path + "'";

    auto* schemaObject = schema.getDynamicObject();
    if (schemaObject == nullptr)
        return {};   // no constraints

    const auto type = schemaObject->getProperty("type").toString().toStdString();

    if (type == "object")
    {
        // var::isObject() is also true for an array, so ask for the object itself.
        auto* object = value.getDynamicObject();
        if (object == nullptr)
            return where + " must be an object, but it is " + describeType(value) + ".";

        const auto required = schemaObject->getProperty("required");
        if (required.isArray())
            for (const auto& name : *required.getArray())
                if (! object->hasProperty(juce::Identifier(name.toString())))
                    return where + " is missing the required property '" + name.toString().toStdString() + "'.";

        const auto properties = schemaObject->getProperty("properties");
        if (auto* propertyObject = properties.getDynamicObject())
        {
            for (const auto& entry : object->getProperties())
            {
                const auto key = entry.name.toString();
                const auto childSchema = propertyObject->getProperty(entry.name);
                if (childSchema.isVoid())
                    continue;   // extra properties are tolerated
                auto problem = validateValue(entry.value, childSchema, path.empty() ? key.toStdString() : path + "." + key.toStdString());
                if (! problem.empty())
                    return problem;
            }
        }
        return {};
    }

    if (type == "array")
    {
        if (! value.isArray())
            return where + " must be an array, but it is " + describeType(value) + ".";
        const auto items = schemaObject->getProperty("items");
        int index = 0;
        for (const auto& element : *value.getArray())
        {
            auto problem = validateValue(element, items, path + "[" + std::to_string(index) + "]");
            if (! problem.empty())
                return problem;
            ++index;
        }
        return {};
    }

    if (type == "string")
    {
        if (! value.isString())
            return where + " must be a string, but it is " + describeType(value) + ".";
    }
    else if (type == "integer")
    {
        if (! isIntegerValue(value))
            return where + " must be an integer, but it is " + describeType(value) + ".";
    }
    else if (type == "number")
    {
        if (! isNumberValue(value))
            return where + " must be a number, but it is " + describeType(value) + ".";
    }
    else if (type == "boolean")
    {
        if (! value.isBool())
            return where + " must be true or false, but it is " + describeType(value) + ".";
    }

    const auto allowed = schemaObject->getProperty("enum");
    if (allowed.isArray())
    {
        bool found = false;
        std::string list;
        for (const auto& option : *allowed.getArray())
        {
            if (! list.empty())
                list += ", ";
            list += option.toString().toStdString();
            if (option == value)
                found = true;
        }
        if (! found)
            return where + " must be one of: " + list + ".";
    }

    if (isNumberValue(value))
    {
        const auto minimum = schemaObject->getProperty("minimum");
        const auto maximum = schemaObject->getProperty("maximum");
        if (isNumberValue(minimum) && (double) value < (double) minimum)
            return where + " must be at least " + minimum.toString().toStdString() + ".";
        if (isNumberValue(maximum) && (double) value > (double) maximum)
            return where + " must be at most " + maximum.toString().toStdString() + ".";
    }
    return {};
}
}

std::string validateArguments(const std::string& schemaJson, const std::string& argumentsJson)
{
    juce::var arguments;
    const auto text = argumentsJson.empty() ? juce::String("{}") : juce::String(argumentsJson);
    if (juce::JSON::parse(text, arguments).failed())
        return "The arguments are not valid JSON.";

    juce::var schema;
    if (juce::JSON::parse(juce::String(schemaJson), schema).failed())
        return "The tool's own schema is not valid JSON (a bug in the tool).";

    return validateValue(arguments, schema, {});
}

void ToolRegistry::add(std::shared_ptr<Tool> tool)
{
    if (tool == nullptr)
        return;
    std::lock_guard<std::mutex> lock(mutex);
    tools[tool->spec().name] = std::move(tool);
}

bool ToolRegistry::remove(const std::string& name)
{
    std::lock_guard<std::mutex> lock(mutex);
    return tools.erase(name) > 0;
}

std::shared_ptr<Tool> ToolRegistry::find(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(mutex);
    const auto found = tools.find(name);
    return found == tools.end() ? nullptr : found->second;
}

std::vector<ToolSpec> ToolRegistry::specs() const
{
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<ToolSpec> result;
    for (const auto& [name, tool] : tools)
        result.push_back(tool->spec());
    return result;
}

namespace
{
struct Running
{
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    ToolResult result;
    CancelToken toolCancel;
};
}

ToolResult ToolRegistry::execute(const ToolCallBlock& call, const CancelToken& runCancel,
                                 const std::function<void(const std::string&)>& progress) const
{
    const auto tool = find(call.name);
    if (tool == nullptr)
        return ToolResult::failure("There is no tool named '" + call.name + "'.");

    const auto problem = validateArguments(tool->spec().parametersSchemaJson, call.argumentsJson);
    if (! problem.empty())
        return ToolResult::failure("Invalid arguments for " + call.name + ": " + problem);

    if (runCancel.isCancelled())
        return ToolResult::failure("The request was stopped before " + call.name + " ran.");

    auto running = std::make_shared<Running>();
    const auto arguments = call.argumentsJson;

    std::thread([tool, running, arguments, progress]
    {
        ToolContext context;
        context.cancel = &running->toolCancel;
        context.progress = progress;

        ToolResult result;
        try
        {
            result = tool->run(arguments, context);
        }
        catch (const std::exception& e)
        {
            result = ToolResult::failure(std::string("The tool failed unexpectedly: ") + e.what());
        }
        catch (...)
        {
            result = ToolResult::failure("The tool failed unexpectedly.");
        }

        std::lock_guard<std::mutex> lock(running->mutex);
        running->result = std::move(result);
        running->done = true;
        running->finished.notify_all();
    }).detach();

    const auto started = std::chrono::steady_clock::now();
    const auto limit = std::chrono::duration<double>(tool->timeoutSeconds());

    std::unique_lock<std::mutex> lock(running->mutex);
    while (! running->done)
    {
        running->finished.wait_for(lock, std::chrono::milliseconds(25));

        if (running->done)
            break;

        if (runCancel.isCancelled())
        {
            running->toolCancel.cancel();
            return ToolResult::failure(call.name + " was stopped.");
        }
        if (std::chrono::steady_clock::now() - started > limit)
        {
            running->toolCancel.cancel();
            return ToolResult::failure(call.name + " timed out after " + std::to_string((int) tool->timeoutSeconds()) + " seconds.");
        }
    }
    return running->result;
}
}
