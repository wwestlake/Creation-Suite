#pragma once

#include "Tool.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace creation::ai
{
// Checks a model's tool arguments against the tool's JSON Schema before the tool runs, so a tool never
// receives malformed input and the model gets a precise error to correct. Supports the part of JSON Schema
// tool arguments use: object, properties, required, string/integer/number/boolean/array/object types,
// enum, minimum/maximum, items. Returns an empty string when valid, otherwise a message naming the problem.
std::string validateArguments(const std::string& schemaJson, const std::string& argumentsJson);

// The set of tools available to the engine. Thread-safe.
class ToolRegistry
{
public:
    // Replaces any tool of the same name.
    void add(std::shared_ptr<Tool> tool);
    bool remove(const std::string& name);

    std::shared_ptr<Tool> find(const std::string& name) const;
    std::vector<ToolSpec> specs() const;   // what is offered to the model

    // Validates the call, runs the tool on its own thread, and returns its result. Never throws. A tool that
    // exceeds its timeout, or whose run is cancelled, gets its context cancelled and a failure result is
    // returned at once (the tool's thread finishes on its own).
    ToolResult execute(const ToolCallBlock& call, const CancelToken& runCancel, const std::function<void(const std::string&)>& progress = nullptr) const;

private:
    mutable std::mutex mutex;
    std::map<std::string, std::shared_ptr<Tool>> tools;
};
}
