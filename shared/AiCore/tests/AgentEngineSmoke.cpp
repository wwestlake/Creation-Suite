// Checks the agent loop against a scripted model: it answers, calls tools and reads their results, corrects
// itself after a failure, and stops cleanly at every limit.

#include <creation/ai/AgentEngine.h>
#include <creation/ai/ScriptedProvider.h>

#include <atomic>
#include <chrono>
#include <iostream>
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

    const std::string emptySchema = R"({"type":"object","properties":{"text":{"type":"string"}}})";

    auto request = [](const std::string& text)
    {
        AgentRequest r;
        r.model = "test";
        r.system = "You are a coder.";
        r.userMessage = text;
        r.limits.retryBaseDelaySeconds = 0.01;
        return r;
    };

    // ---- A plain answer ----
    {
        ScriptedProvider provider;
        provider.addTextReply("Hello there.");
        ToolRegistry tools;
        AgentEngine engine(provider, tools);
        CancelToken cancel;

        std::string streamed;
        auto r = request("hi");
        r.events.onText = [&](const std::string& t) { streamed += t; };
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::finished && result.finalText == "Hello there." && result.steps == 1, "a plain answer finishes in one step");
        check(streamed == "Hello there.", "the reply text is streamed to the host");
        check(result.messages.size() == 2 && result.messages[0].role == Role::user && result.messages[1].role == Role::assistant,
              "the run's messages are the user message and the reply");
    }

    // ---- A tool call, its result fed back, then an answer ----
    {
        ScriptedProvider provider;
        provider.addToolCallReply("call_1", "echo", R"({"text":"hello"})", "Checking.");
        provider.addTextReply("It said hello.");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "echo", "Echo", emptySchema }, Effect::read,
                                                 [](const std::string& args, ToolContext&) { return ToolResult::success("echoed " + args); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;

        std::vector<std::string> log;
        auto r = request("say hello");
        r.events.onToolCall = [&](const ToolCallBlock& c) { log.push_back("call:" + c.name); };
        r.events.onToolResult = [&](const ToolCallBlock& c, const ToolResult& t) { log.push_back(std::string("result:") + (t.ok ? "ok" : "fail")); };
        r.events.onStepStarted = [&](int s) { log.push_back("step" + std::to_string(s)); };
        auto result = engine.run(r, cancel);

        check(result.reason == EndReason::finished && result.finalText == "It said hello." && result.steps == 2 && result.toolCalls == 1,
              "a tool call is run and the model then answers");
        check(log == std::vector<std::string> { "step1", "call:echo", "result:ok", "step2" }, "events arrive in order");

        auto received = provider.receivedRequests();
        check(received.size() == 2 && received[0].tools.size() == 1 && received[0].system == "You are a coder.",
              "the model was offered the registry's tools and the system prompt");
        check(received[1].messages.size() == 3 && received[1].messages[2].role == Role::tool, "the second request carries the tool result");
        const auto& block = std::get<ToolResultBlock>(received[1].messages[2].content[0]);
        check(block.callId == "call_1" && contains(block.content, "echoed") && ! block.isError, "the result quotes the call id and content");
    }

    // ---- Write, fail, read the error, correct ----
    {
        ScriptedProvider provider;
        provider.addToolCallReply("c1", "compile", R"({"text":"broken"})");
        provider.addToolCallReply("c2", "compile", R"({"text":"fixed"})");
        provider.addTextReply("It compiles now.");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "compile", "Compile", emptySchema }, Effect::write,
                                                 [](const std::string& args, ToolContext&)
                                                 {
                                                     return contains(args, "broken") ? ToolResult::failure("script.frust:3:11: error: syntax error")
                                                                                     : ToolResult::success("Compiled cleanly.");
                                                 }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto result = engine.run(request("make it compile"), cancel);
        check(result.reason == EndReason::finished && result.toolCalls == 2, "the model can correct itself after a failed call");
        const auto sentRequests = provider.receivedRequests();
        const auto& seen = std::get<ToolResultBlock>(sentRequests[1].messages[2].content[0]);
        check(seen.isError && contains(seen.content, "3:11"), "the model was shown the error, marked as an error");
    }

    // ---- Step limit ----
    {
        ScriptedProvider provider;
        for (int i = 0; i < 10; ++i)
            provider.addToolCallReply("c" + std::to_string(i), "echo", "{\"text\":\"" + std::to_string(i) + "\"}");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "echo", "Echo", emptySchema }, Effect::read,
                                                 [](const std::string&, ToolContext&) { return ToolResult::success("ok"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto r = request("go");
        r.limits.maxSteps = 4;
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::stepLimit && result.steps == 4, "the step limit ends the run");
    }

    // ---- Loop detection ----
    {
        ScriptedProvider provider;
        for (int i = 0; i < 10; ++i)
            provider.addToolCallReply("c" + std::to_string(i), "echo", R"({"text":"same"})");
        int runs = 0;
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "echo", "Echo", emptySchema }, Effect::read,
                                                 [&](const std::string&, ToolContext&) { ++runs; return ToolResult::success("ok"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto result = engine.run(request("go"), cancel);
        check(result.reason == EndReason::loopDetected, "repeating one call is detected as a loop");
        check(runs == 2, "the repeated call ran only up to its allowance");
        bool told = false;
        for (const auto& message : result.messages)
            for (const auto& block : message.content)
                if (const auto* r = std::get_if<ToolResultBlock>(&block))
                    if (r->isError && contains(r->content, "exact call"))
                        told = true;
        check(told, "the model was told it was repeating itself");
    }

    // ---- Failing steps in a row ----
    {
        ScriptedProvider provider;
        for (int i = 0; i < 10; ++i)
            provider.addToolCallReply("c" + std::to_string(i), "fail", "{\"text\":\"" + std::to_string(i) + "\"}");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "fail", "Fail", emptySchema }, Effect::write,
                                                 [](const std::string&, ToolContext&) { return ToolResult::failure("nope"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto result = engine.run(request("go"), cancel);
        check(result.reason == EndReason::tooManyFailures, "steps of nothing but failures end the run");
    }

    // ---- Provider errors ----
    {
        ScriptedProvider provider;
        provider.addError(ErrorKind::rateLimited, "slow down", 0.01);
        provider.addError(ErrorKind::serverError, "oops");
        provider.addTextReply("Finally.");
        ToolRegistry tools;
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        std::vector<std::string> notices;
        auto r = request("hi");
        r.events.onNotice = [&](const std::string& n) { notices.push_back(n); };
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::finished && result.finalText == "Finally.", "retryable errors are retried");
        check(notices.size() == 2 && contains(notices[0], "slow down"), "each retry is announced");
    }
    {
        ScriptedProvider provider;
        provider.addError(ErrorKind::authentication, "bad key");
        provider.addTextReply("never");
        ToolRegistry tools;
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto result = engine.run(request("hi"), cancel);
        check(result.reason == EndReason::providerError && contains(result.detail, "bad key") && provider.repliesRemaining() == 1,
              "a bad key ends the run at once, without retrying");
    }
    {
        ScriptedProvider provider;
        for (int i = 0; i < 6; ++i)
            provider.addError(ErrorKind::network, "down");
        ToolRegistry tools;
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto r = request("hi");
        r.limits.maxProviderRetries = 2;
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::providerError && provider.receivedRequests().size() == 3, "retries stop after the allowed number");
    }

    // ---- Stopping ----
    {
        ScriptedProvider provider;
        provider.addToolCallReply("c1", "slow", "{}");
        provider.addTextReply("never");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "slow", "Slow", R"({"type":"object"})" }, Effect::read,
                                                 [](const std::string&, ToolContext& ctx)
                                                 {
                                                     for (int i = 0; i < 400 && ! ctx.cancelled(); ++i)
                                                         std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                                     return ToolResult::success("late");
                                                 }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        std::thread stopper([&] { std::this_thread::sleep_for(std::chrono::milliseconds(80)); cancel.cancel(); });
        auto result = engine.run(request("go"), cancel);
        stopper.join();
        check(result.reason == EndReason::stopped, "stopping during a tool ends the run as stopped");
        check(result.messages.back().role == Role::tool, "the conversation still holds a result for the call that was in flight");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ---- Permission policy ----
    {
        ScriptedProvider provider;
        provider.addToolCallReply("c1", "delete_all", "{}");
        provider.addTextReply("Understood, I will not.");
        int runs = 0;
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "delete_all", "Delete", R"({"type":"object"})" }, Effect::destructive,
                                                 [&](const std::string&, ToolContext&) { ++runs; return ToolResult::success("deleted"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto r = request("clean up");
        r.policy = [](const Tool& tool, const ToolCallBlock&) -> ToolDecision
        {
            if (tool.effect() == Effect::destructive)
                return { false, "the user declined" };
            return {};
        };
        auto result = engine.run(r, cancel);
        check(runs == 0 && result.reason == EndReason::finished, "a denied tool never runs, and the run continues");
        const auto sentRequests = provider.receivedRequests();
        const auto& seen = std::get<ToolResultBlock>(sentRequests[1].messages[2].content[0]);
        check(seen.isError && contains(seen.content, "the user declined"), "the model is told why it was denied");
    }

    // ---- Too many calls in one reply ----
    {
        ScriptedProvider provider;
        ChatResponse many;
        many.assistant.role = Role::assistant;
        for (int i = 0; i < 4; ++i)
            many.assistant.content.push_back(ToolCallBlock { "c" + std::to_string(i), "echo", "{\"text\":\"" + std::to_string(i) + "\"}" });
        many.stopReason = StopReason::toolUse;
        provider.addResponse(many);
        provider.addTextReply("done");
        int runs = 0;
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "echo", "Echo", emptySchema }, Effect::read,
                                                 [&](const std::string&, ToolContext&) { ++runs; return ToolResult::success("ok"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto r = request("go");
        r.limits.maxToolCallsPerStep = 2;
        auto result = engine.run(r, cancel);
        check(runs == 2, "calls beyond the per-step limit are not run");
        const auto sentRequests = provider.receivedRequests();
        const auto& second = sentRequests[1].messages.back();
        check(second.content.size() == 4, "every call still gets a result, so the conversation stays valid");
    }

    // ---- A model that cannot call tools ----
    {
        ScriptedProvider provider({ false, false, true, 0 });
        provider.addTextReply("Plain answer.");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "echo", "Echo", emptySchema }, Effect::read,
                                                 [](const std::string&, ToolContext&) { return ToolResult::success("ok"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        std::vector<std::string> notices;
        auto r = request("hi");
        r.events.onNotice = [&](const std::string& n) { notices.push_back(n); };
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::finished && provider.receivedRequests()[0].tools.empty(), "tools are not offered to a model that cannot call them");
        check(notices.size() == 1, "the host is told why no tools were offered");
    }

    // ---- Cut off by the output limit: asked to continue ----
    {
        ScriptedProvider provider;
        ChatResponse cut;
        cut.assistant = Message::text(Role::assistant, "The first half ");
        cut.stopReason = StopReason::maxTokens;
        provider.addResponse(cut);
        provider.addTextReply("and the second half.");
        ToolRegistry tools;
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto result = engine.run(request("write"), cancel);
        check(result.reason == EndReason::finished && result.finalText == "and the second half." && result.messages.size() == 4,
              "a reply cut off by the output limit is continued");
    }

    // ---- Token limit and usage ----
    {
        ScriptedProvider provider;
        for (int i = 0; i < 5; ++i)
        {
            ChatResponse response;
            response.assistant.role = Role::assistant;
            response.assistant.content.push_back(ToolCallBlock { "c" + std::to_string(i), "echo", "{\"text\":\"" + std::to_string(i) + "\"}" });
            response.stopReason = StopReason::toolUse;
            response.usage = { 400, 100 };
            provider.addResponse(response);
        }
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "echo", "Echo", emptySchema }, Effect::read,
                                                 [](const std::string&, ToolContext&) { return ToolResult::success("ok"); }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto r = request("go");
        r.limits.maxTotalTokens = 1200;
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::tokenLimit && result.usage.inputTokens == 1200 && result.usage.outputTokens == 300,
              "usage adds up and the token limit ends the run");
    }

    // ---- Time limit ----
    {
        ScriptedProvider provider;
        for (int i = 0; i < 100; ++i)
            provider.addToolCallReply("c" + std::to_string(i), "nap", "{\"text\":\"" + std::to_string(i) + "\"}");
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(ToolSpec { "nap", "Nap", emptySchema }, Effect::read,
                                                 [](const std::string&, ToolContext&)
                                                 {
                                                     std::this_thread::sleep_for(std::chrono::milliseconds(30));
                                                     return ToolResult::success("ok");
                                                 }));
        AgentEngine engine(provider, tools);
        CancelToken cancel;
        auto r = request("go");
        r.limits.maxSeconds = 0.2;
        r.limits.maxSteps = 100;
        auto result = engine.run(r, cancel);
        check(result.reason == EndReason::timeLimit, "the time limit ends a long run");
    }

    // ---- Trimming keeps tool exchanges whole ----
    {
        std::vector<Message> messages;
        messages.push_back(Message::text(Role::user, "the original request"));
        for (int i = 0; i < 6; ++i)
        {
            Message call;
            call.role = Role::assistant;
            call.content.push_back(ToolCallBlock { "c" + std::to_string(i), "echo", "{}" });
            messages.push_back(call);
            messages.push_back(Message::toolResult("c" + std::to_string(i), std::string(1000, 'x')));
        }
        messages.push_back(Message::text(Role::user, "and the latest question"));

        const auto before = messages.size();
        const int removed = AgentEngine::trimToFit(messages, 3500);
        check(removed > 0 && messages.size() < before && AgentEngine::sizeOf(messages) <= 3500 + 200, "an oversized conversation is trimmed");
        check(std::get<TextBlock>(messages.front().content[0]).text == "the original request", "the first message is kept");
        check(messages.back().plainText() == "and the latest question", "the latest message is kept");
        check(contains(messages[1].plainText(), "earlier messages were removed"), "a marker says messages were removed");

        bool paired = true;
        for (size_t i = 0; i < messages.size(); ++i)
        {
            if (! messages[i].toolCalls().empty())
                paired = paired && i + 1 < messages.size() && messages[i + 1].role == Role::tool;
            if (messages[i].role == Role::tool)
                paired = paired && i > 0 && ! messages[i - 1].toolCalls().empty();
        }
        check(paired, "no tool call was separated from its result");
        check(AgentEngine::trimToFit(messages, 1000000) == 0, "a conversation that fits is left alone");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
