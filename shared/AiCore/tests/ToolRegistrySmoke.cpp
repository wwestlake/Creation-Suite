// Checks argument validation and the tool registry: what a model sends is validated before a tool runs,
// and a tool that fails, hangs or is stopped never takes the engine down with it.

#include <creation/ai/ToolRegistry.h>

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}
}

int main()
{
    using namespace creation::ai;

    // ---- Argument validation ----
    {
        const std::string schema = R"({
            "type":"object",
            "properties":{
                "code":{"type":"string"},
                "track":{"type":"integer","minimum":1,"maximum":64},
                "mode":{"type":"string","enum":["a","b"]},
                "points":{"type":"array","items":{"type":"object","properties":{"time":{"type":"number"}},"required":["time"]}}
            },
            "required":["code"]
        })";

        check(validateArguments(schema, R"({"code":"x"})").empty(), "valid arguments pass");
        check(validateArguments(schema, R"({"code":"x","track":3,"mode":"a"})").empty(), "valid optional arguments pass");
        check(contains(validateArguments(schema, "{}"), "missing the required property 'code'"), "a missing required property is named");
        check(contains(validateArguments(schema, R"({"code":5})"), "'code' must be a string"), "a wrong type is named with its path");
        check(contains(validateArguments(schema, R"({"code":"x","track":0})"), "at least 1"), "a value below the minimum is rejected");
        check(contains(validateArguments(schema, R"({"code":"x","track":99})"), "at most 64"), "a value above the maximum is rejected");
        check(contains(validateArguments(schema, R"({"code":"x","track":1.5})"), "must be an integer"), "a fraction is not an integer");
        check(contains(validateArguments(schema, R"({"code":"x","mode":"z"})"), "one of: a, b"), "an enum value outside the list is rejected");
        check(contains(validateArguments(schema, R"({"code":"x","points":[{"time":1},{}]})"), "'points[1]'"), "errors inside arrays name the element");
        check(contains(validateArguments(schema, "not json"), "not valid JSON"), "malformed JSON is reported");
        check(contains(validateArguments(schema, "[1]"), "must be an object"), "arguments must be an object");
        check(validateArguments(schema, R"({"code":"x","extra":1})").empty(), "extra properties are tolerated");
        check(validateArguments(R"({"type":"object","properties":{}})", "").empty(), "empty arguments count as an empty object");
    }

    // ---- Tool registry ----
    {
        ToolRegistry registry;
        int runs = 0;

        registry.add(std::make_shared<FunctionTool>(
            ToolSpec { "echo", "Echo text", R"({"type":"object","properties":{"text":{"type":"string"}},"required":["text"]})" },
            Effect::read,
            [&](const std::string& args, ToolContext&) { ++runs; return ToolResult::success("got " + args); }));

        registry.add(std::make_shared<FunctionTool>(
            ToolSpec { "boom", "Throws", R"({"type":"object"})" }, Effect::write,
            [](const std::string&, ToolContext&) -> ToolResult { throw std::runtime_error("kaboom"); }));

        registry.add(std::make_shared<FunctionTool>(
            ToolSpec { "slow", "Runs until cancelled", R"({"type":"object"})" }, Effect::read,
            [](const std::string&, ToolContext& ctx)
            {
                for (int i = 0; i < 400 && ! ctx.cancelled(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return ToolResult::success(ctx.cancelled() ? "cancelled" : "finished");
            },
            0.2));

        check(registry.specs().size() == 3, "the registry offers its tools' specs");
        check(registry.find("echo") != nullptr && registry.find("missing") == nullptr, "find returns a tool or nothing");

        CancelToken noCancel;
        auto ok = registry.execute({ "c1", "echo", R"({"text":"hi"})" }, noCancel);
        check(ok.ok && contains(ok.content, "hi") && runs == 1, "a valid call runs the tool");

        auto invalid = registry.execute({ "c2", "echo", "{}" }, noCancel);
        check(! invalid.ok && contains(invalid.content, "Invalid arguments for echo") && runs == 1,
              "an invalid call never reaches the tool");

        auto unknown = registry.execute({ "c3", "nope", "{}" }, noCancel);
        check(! unknown.ok && contains(unknown.content, "no tool named 'nope'"), "an unknown tool is a readable failure");

        auto thrown = registry.execute({ "c4", "boom", "{}" }, noCancel);
        check(! thrown.ok && contains(thrown.content, "kaboom"), "an exception in a tool becomes a failure result");

        const auto started = std::chrono::steady_clock::now();
        auto timedOut = registry.execute({ "c5", "slow", "{}" }, noCancel);
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        check(! timedOut.ok && contains(timedOut.content, "timed out") && elapsed < 2.0, "a tool past its timeout is stopped and reported");

        CancelToken stop;
        std::thread stopper([&] { std::this_thread::sleep_for(std::chrono::milliseconds(60)); stop.cancel(); });
        auto stopped = registry.execute({ "c6", "slow", "{}" }, stop);
        stopper.join();
        check(! stopped.ok && contains(stopped.content, "stopped"), "stopping the run stops a running tool");

        auto afterStop = registry.execute({ "c7", "echo", R"({"text":"x"})" }, stop);
        check(! afterStop.ok && runs == 1, "a stopped run does not start another tool");

        check(registry.remove("boom") && registry.find("boom") == nullptr, "a tool can be removed");

        std::this_thread::sleep_for(std::chrono::milliseconds(300));   // let the detached tool threads finish
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
