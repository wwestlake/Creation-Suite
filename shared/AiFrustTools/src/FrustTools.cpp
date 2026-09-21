#include "creation/ai/FrustTools.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace creation::ai
{
namespace
{
const char* codeSchema = R"({
    "type": "object",
    "properties": {
        "code": { "type": "string", "description": "The complete FRust source of the script." }
    },
    "required": ["code"]
})";

// Pulls the "code" argument out of the tool's JSON arguments.
bool readCode(const std::string& argumentsJson, std::string& code)
{
    juce::var parsed;
    if (juce::JSON::parse(juce::String(argumentsJson), parsed).failed())
        return false;
    auto* object = parsed.getDynamicObject();
    if (object == nullptr || ! object->hasProperty("code"))
        return false;
    code = object->getProperty("code").toString().toStdString();
    return true;
}

class RunFrustTool final : public Tool
{
public:
    explicit RunFrustTool(creation::frust::ScriptRunner& scriptRunner) : runner(scriptRunner) {}

    ToolSpec spec() const override
    {
        return { "run_frust",
                 "Compile and run a FRust script against the application. The script must define `pub fn run() -> String`; "
                 "what it returns, and anything it logs with the API's log function, is reported back. Changes the script makes "
                 "to the project are real. If the script does not compile you get the errors with file, line and column: fix "
                 "them and run again.",
                 codeSchema };
    }

    Effect effect() const override { return Effect::write; }
    double timeoutSeconds() const override { return 120.0; }

    ToolResult run(const std::string& argumentsJson, ToolContext& ctx) override
    {
        std::string code;
        if (! readCode(argumentsJson, code))
            return ToolResult::failure("The arguments must be an object with the script in `code`.");

        std::mutex mutex;
        std::condition_variable finished;
        bool done = false;
        creation::frust::ScriptResult result;

        const bool started = runner.runAsync(code, [&](creation::frust::ScriptResult r)
        {
            std::lock_guard<std::mutex> lock(mutex);
            result = std::move(r);
            done = true;
            finished.notify_all();
        });
        if (! started)
            return ToolResult::failure("A script is already running. Wait for it to finish.");

        std::unique_lock<std::mutex> lock(mutex);
        while (! done)
        {
            finished.wait_for(lock, std::chrono::milliseconds(20));
            if (! done && ctx.cancelled())
                runner.requestStop();
        }

        const bool ok = result.ok;
        return ok ? ToolResult::success(describeScriptResult(result)) : ToolResult::failure(describeScriptResult(result));
    }

private:
    creation::frust::ScriptRunner& runner;
};

class CheckFrustTool final : public Tool
{
public:
    explicit CheckFrustTool(creation::frust::ScriptRunner& scriptRunner) : runner(scriptRunner) {}

    ToolSpec spec() const override
    {
        return { "check_frust",
                 "Compile a FRust script without running it, and report any errors with file, line and column.",
                 codeSchema };
    }

    Effect effect() const override { return Effect::read; }

    ToolResult run(const std::string& argumentsJson, ToolContext&) override
    {
        std::string code;
        if (! readCode(argumentsJson, code))
            return ToolResult::failure("The arguments must be an object with the script in `code`.");

        const auto result = runner.check(code);
        return result.ok ? ToolResult::success("No errors. The script compiles.") : ToolResult::failure(describeScriptResult(result));
    }

private:
    creation::frust::ScriptRunner& runner;
};

class LookupTool final : public Tool
{
public:
    explicit LookupTool(std::function<std::string(const std::string&)> searchFunction) : search(std::move(searchFunction)) {}

    ToolSpec spec() const override
    {
        return { "frust_lookup",
                 "Search the help for how to write FRust and use the application's script API. Use it when you are unsure of the "
                 "syntax, an error message, or what a function does, instead of guessing. Ask a specific question.",
                 R"({"type":"object","properties":{"query":{"type":"string","description":"What you want to know, e.g. how to loop over tracks, or what 'expecting }' means."}},"required":["query"]})" };
    }

    Effect effect() const override { return Effect::read; }

    ToolResult run(const std::string& argumentsJson, ToolContext&) override
    {
        juce::var parsed;
        std::string query;
        if (! juce::JSON::parse(juce::String(argumentsJson), parsed).failed())
            if (auto* object = parsed.getDynamicObject())
                query = object->getProperty("query").toString().toStdString();
        if (query.empty())
            return ToolResult::failure("Give a query: what do you want to know?");

        const auto found = search ? search(query) : std::string();
        if (found.empty())
            return ToolResult::success("Nothing in the help matches that. Try different words, or use frust_api_reference.");
        return ToolResult::success(found);
    }

private:
    std::function<std::string(const std::string&)> search;
};

class ApiReferenceTool final : public Tool
{
public:
    explicit ApiReferenceTool(const creation::frust::ScriptApi& api)
    {
        text = "The application's API for scripts (FRust `extern fn` declarations, reached with `use self::" + api.moduleName + ";` "
               "which is already done for you):\n\n" + api.declarations;
        if (! api.modules.empty())
        {
            text += "\n\nPremade helper modules you can bring in with `use self::<name>;`:\n";
            for (const auto& module : api.modules)
                text += "\n---- " + module.name + " ----\n" + module.source;
        }
    }

    ToolSpec spec() const override
    {
        return { "frust_api_reference", "Show the application's script API and any premade helper modules.", R"({"type":"object","properties":{}})" };
    }

    Effect effect() const override { return Effect::read; }

    ToolResult run(const std::string&, ToolContext&) override { return ToolResult::success(text); }

private:
    std::string text;
};
}

std::string repairHint(const std::string& message)
{
    const auto has = [&](const char* part) { return message.find(part) != std::string::npos; };

    if (has("unexpected !"))
        return "FRust has no macros (no format!, println!). Return a string literal, and report numbers with the API's log functions.";
    if (has("cannot call a method on"))
        return "Values have no methods in FRust (no .to_string(), .len(), ...). Use plain functions from the API.";
    if (has("unexpected &"))
        return "FRust has no && or ||. Nest `if` blocks, or combine parenthesized comparisons with & (and) / | (or): if ((a == 1) & (b == 2)) { ... }";
    if (has("is not defined for these operands"))
        return "Operators work on numbers only. There is no String concatenation; report text with separate log calls.";
    if (has("has no return value"))
        return "The last line of a function must be an expression giving its value, with no `;` after it.";
    if (has("expecting }") || has("expecting ;"))
        return "In a block, every statement except the last ends with `;`, including `if`/`else` and `while` blocks (write `};`). "
               "The last expression is the block's value and has no `;`. The real mistake is usually on the line BEFORE the one reported. "
               "Also check every `if`/`while` condition: it must be in parentheses, `if (ok) { ... }`, because a bare name before `{` is read "
               "as a struct value and gives this same error.";
    if (has("is not defined for this operand"))
        return "That operator works on numbers only (`!` also on bool). Text cannot be negated or combined.";
    return {};
}

std::string describeScriptResult(const creation::frust::ScriptResult& result)
{
    std::string text;

    if (! result.diagnostics.empty())
    {
        text += "The script did not compile:\n";
        std::vector<std::string> hints;
        for (const auto& diagnostic : result.diagnostics)
        {
            text += "  " + ::frust::FormatDiagnostic(diagnostic) + "\n";
            const auto hint = repairHint(diagnostic.message);
            if (! hint.empty() && std::find(hints.begin(), hints.end(), hint) == hints.end())
                hints.push_back(hint);
        }
        for (const auto& hint : hints)
            text += "Hint: " + hint + "\n";
        return text;
    }

    if (! result.error.empty())
        return "The script could not run: " + result.error;

    text = "The script ran.";
    if (result.stopped)
        text += " It was stopped, or ran past its time limit, before it finished on its own.";
    if (! result.output.empty())
        text += "\nOutput:\n" + result.output;
    return text;
}

std::shared_ptr<Tool> makeRunFrustTool(creation::frust::ScriptRunner& runner)
{
    return std::make_shared<RunFrustTool>(runner);
}

std::shared_ptr<Tool> makeCheckFrustTool(creation::frust::ScriptRunner& runner)
{
    return std::make_shared<CheckFrustTool>(runner);
}

std::shared_ptr<Tool> makeFrustLookupTool(std::function<std::string(const std::string& query)> search)
{
    return std::make_shared<LookupTool>(std::move(search));
}

std::shared_ptr<Tool> makeFrustApiReferenceTool(const creation::frust::ScriptApi& api)
{
    return std::make_shared<ApiReferenceTool>(api);
}
}
