#pragma once

#include "Messages.h"
#include "Provider.h"

#include <functional>
#include <memory>
#include <string>

namespace creation::ai
{
// What running a tool can do, so a policy can decide whether to allow it or ask first.
enum class Effect
{
    read,          // looks at things only
    write,         // changes the project; undoable
    destructive,   // discards something that cannot be got back
    external       // reaches beyond the project: network, other programs
};

struct ToolResult
{
    bool ok = true;
    std::string content;    // what the model reads: the answer, or the reason it failed

    static ToolResult success(std::string text) { return { true, std::move(text) }; }
    static ToolResult failure(std::string why) { return { false, std::move(why) }; }
};

// Passed to a running tool.
struct ToolContext
{
    const CancelToken* cancel = nullptr;                  // set when the run is stopped or the tool times out
    std::function<void(const std::string&)> progress;     // optional: a line the host may show while it runs

    bool cancelled() const noexcept { return cancel != nullptr && cancel->isCancelled(); }
};

// A capability the model can use. Tools are registered by the suite and by each app; the engine knows only
// this interface.
class Tool
{
public:
    virtual ~Tool() = default;

    virtual ToolSpec spec() const = 0;
    virtual Effect effect() const { return Effect::read; }
    virtual double timeoutSeconds() const { return 60.0; }

    // `argumentsJson` has already been checked against spec().parametersSchemaJson. Run on a worker
    // thread; check ctx.cancelled() during long work. Report failure as a failure result, not an exception.
    virtual ToolResult run(const std::string& argumentsJson, ToolContext& ctx) = 0;
};

// A tool made from a function, for simple cases and tests.
class FunctionTool final : public Tool
{
public:
    using Fn = std::function<ToolResult(const std::string& argumentsJson, ToolContext&)>;

    FunctionTool(ToolSpec toolSpec, Effect toolEffect, Fn function, double timeout = 60.0)
        : specification(std::move(toolSpec)), effectClass(toolEffect), fn(std::move(function)), timeout(timeout) {}

    ToolSpec spec() const override { return specification; }
    Effect effect() const override { return effectClass; }
    double timeoutSeconds() const override { return timeout; }
    ToolResult run(const std::string& argumentsJson, ToolContext& ctx) override { return fn(argumentsJson, ctx); }

private:
    ToolSpec specification;
    Effect effectClass;
    Fn fn;
    double timeout;
};
}
