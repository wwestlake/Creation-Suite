// The OpenAI wire format and provider, without a network: the request the provider builds, how each kind of
// reply and error is understood, and the provider working inside the agent loop with a fake transport.

#include <creation/ai/AgentEngine.h>
#include <creation/ai/OpenAiProvider.h>

#include <juce_core/juce_core.h>

#include <iostream>
#include <mutex>

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

juce::var parse(const std::string& json)
{
    juce::var v;
    juce::JSON::parse(juce::String(json), v);
    return v;
}

// A transport that returns canned replies and remembers what it was sent.
class FakeHttp final : public creation::ai::HttpClient
{
public:
    std::vector<creation::ai::HttpRequest> sent;
    std::vector<creation::ai::HttpResponse> replies;
    size_t next = 0;

    creation::ai::HttpResponse post(const creation::ai::HttpRequest& request, const creation::ai::CancelToken&) override
    {
        sent.push_back(request);
        return next < replies.size() ? replies[next++] : creation::ai::HttpResponse {};
    }
};

creation::ai::HttpResponse ok(const std::string& body)
{
    creation::ai::HttpResponse r;
    r.status = 200;
    r.body = body;
    return r;
}

std::string headerValue(const creation::ai::HttpRequest& request, const std::string& name)
{
    for (const auto& [key, value] : request.headers)
        if (key == name)
            return value;
    return {};
}
}

int main()
{
    using namespace creation::ai;

    // ---- Building the request ----
    {
        ChatRequest request;
        request.model = "gpt-test";
        request.system = "You write FRust.";
        request.messages.push_back(Message::text(Role::user, "add a track"));

        Message assistant;
        assistant.role = Role::assistant;
        assistant.content.push_back(TextBlock { "Adding it." });
        assistant.content.push_back(ToolCallBlock { "call_1", "run_frust", R"({"code":"x"})" });
        request.messages.push_back(assistant);

        Message results;
        results.role = Role::tool;
        results.content.push_back(ToolResultBlock { "call_1", "ok", false });
        request.messages.push_back(results);

        request.tools.push_back({ "run_frust", "Run FRust", R"({"type":"object","properties":{"code":{"type":"string"}},"required":["code"]})" });
        request.temperature = 0.3;
        request.maxOutputTokens = 1234;

        const auto body = parse(openai::buildRequestBody(request, {}));
        auto* root = body.getDynamicObject();
        check(root != nullptr && root->getProperty("model").toString() == "gpt-test", "the body names the model");

        const auto* messages = root->getProperty("messages").getArray();
        check(messages != nullptr && messages->size() == 4, "system, user, assistant and tool result become four messages");
        auto role = [&](int i) { return (*messages)[i].getDynamicObject()->getProperty("role").toString().toStdString(); };
        check(role(0) == "system" && role(1) == "user" && role(2) == "assistant" && role(3) == "tool", "the roles are in order");

        auto* assistantWire = (*messages)[2].getDynamicObject();
        check(assistantWire->getProperty("content").toString() == "Adding it.", "the assistant's text is kept");
        auto* firstCall = (*assistantWire->getProperty("tool_calls").getArray())[0].getDynamicObject();
        check(firstCall->getProperty("id").toString() == "call_1" && firstCall->getProperty("type").toString() == "function"
                  && firstCall->getProperty("function").getDynamicObject()->getProperty("name").toString() == "run_frust",
              "the assistant's tool call is sent with its id and name");
        check(contains(firstCall->getProperty("function").getDynamicObject()->getProperty("arguments").toString().toStdString(), "\"code\""),
              "the call's arguments are a JSON string");

        auto* toolWire = (*messages)[3].getDynamicObject();
        check(toolWire->getProperty("tool_call_id").toString() == "call_1" && toolWire->getProperty("content").toString() == "ok",
              "a tool result quotes its call id");

        const auto* tools = root->getProperty("tools").getArray();
        check(tools != nullptr && tools->size() == 1, "the tools are offered");
        auto* function = (*tools)[0].getDynamicObject()->getProperty("function").getDynamicObject();
        check(function->getProperty("name").toString() == "run_frust"
                  && function->getProperty("parameters").getDynamicObject()->getProperty("type").toString() == "object",
              "a tool carries its JSON schema as its parameters");
        check(root->hasProperty("max_tokens") && (int) root->getProperty("max_tokens") == 1234 && ! root->hasProperty("max_completion_tokens"),
              "the output limit is max_tokens by default");
        check(root->hasProperty("temperature"), "the temperature is sent by default");

        openai::WireOptions modern;
        modern.useMaxCompletionTokens = true;
        modern.sendTemperature = false;
        const auto modernBody = parse(openai::buildRequestBody(request, modern));   // keep the value alive: the object belongs to it
        auto* modernRoot = modernBody.getDynamicObject();
        check(modernRoot->hasProperty("max_completion_tokens") && ! modernRoot->hasProperty("max_tokens") && ! modernRoot->hasProperty("temperature"),
              "the options switch to max_completion_tokens and drop the temperature");

        ChatRequest plain;
        plain.model = "m";
        plain.messages.push_back(Message::text(Role::user, "hi"));
        check(! parse(openai::buildRequestBody(plain, {})).getDynamicObject()->hasProperty("tools"), "no tools means no tools field");

        Message calls;
        calls.role = Role::assistant;
        calls.content.push_back(ToolCallBlock { "a", "t", "{}" });
        plain.messages.push_back(calls);
        const auto callsBody = parse(openai::buildRequestBody(plain, {}));
        const auto callsMessages = callsBody.getDynamicObject()->getProperty("messages");
        auto* callsWire = (*callsMessages.getArray())[1].getDynamicObject();
        check(callsWire->getProperty("content").isVoid(), "an assistant message that is only tool calls has null content");
    }

    // ---- Understanding replies ----
    {
        auto text = openai::parseResponse(ok(R"({"choices":[{"message":{"role":"assistant","content":"Hello"},"finish_reason":"stop"}],
                                                  "usage":{"prompt_tokens":12,"completion_tokens":3}})"));
        check(text.ok() && text.assistant.plainText() == "Hello" && text.stopReason == StopReason::endOfTurn, "a text reply is read");
        check(text.usage.inputTokens == 12 && text.usage.outputTokens == 3, "token usage is read");

        auto tool = openai::parseResponse(ok(R"({"choices":[{"message":{"role":"assistant","content":null,
                                                  "tool_calls":[{"id":"call_9","type":"function","function":{"name":"run_frust","arguments":"{\"code\":\"x\"}"}}]},
                                                  "finish_reason":"tool_calls"}]})"));
        check(tool.ok() && tool.stopReason == StopReason::toolUse && tool.assistant.toolCalls().size() == 1, "a tool-call reply is read");
        check(tool.assistant.toolCalls()[0].id == "call_9" && tool.assistant.toolCalls()[0].name == "run_frust"
                  && contains(tool.assistant.toolCalls()[0].argumentsJson, "\"code\""),
              "the call's id, name and arguments are read");

        auto both = openai::parseResponse(ok(R"({"choices":[{"message":{"content":"Let me do that.",
                                                  "tool_calls":[{"id":"c","type":"function","function":{"name":"t","arguments":"{}"}}]},"finish_reason":"tool_calls"}]})"));
        check(both.assistant.plainText() == "Let me do that." && both.assistant.toolCalls().size() == 1, "text and a call in one reply are both kept");

        auto cut = openai::parseResponse(ok(R"({"choices":[{"message":{"content":"partial"},"finish_reason":"length"}]})"));
        check(cut.stopReason == StopReason::maxTokens, "a length finish is a max-tokens stop");
    }

    // ---- Understanding errors ----
    {
        HttpResponse r;
        r.status = 401;
        r.body = R"({"error":{"message":"Incorrect API key provided","type":"invalid_request_error","code":"invalid_api_key"}})";
        auto auth = openai::parseResponse(r);
        check(! auth.ok() && auth.error->kind == ErrorKind::authentication && contains(auth.error->message, "Incorrect API key") && ! auth.error->retryable(),
              "401 is an authentication error, with the provider's words, not retryable");

        r.status = 429;
        r.retryAfter = "7";
        r.body = R"({"error":{"message":"Rate limit reached"}})";
        auto limited = openai::parseResponse(r);
        check(limited.error->kind == ErrorKind::rateLimited && limited.error->retryAfterSeconds == 7.0 && limited.error->retryable(),
              "429 is a rate limit with the delay the provider gave");

        r.status = 503;
        r.retryAfter.clear();
        r.body = "overloaded";
        check(openai::parseResponse(r).error->kind == ErrorKind::serverError, "5xx is a server error, even without JSON");

        r.status = 400;
        r.body = R"({"error":{"message":"This model's maximum context length is 8192 tokens","code":"context_length_exceeded"}})";
        check(openai::parseResponse(r).error->kind == ErrorKind::contextTooLong, "a context-length error is recognised");

        r.body = R"({"error":{"message":"Invalid parameter"}})";
        check(openai::parseResponse(r).error->kind == ErrorKind::badRequest, "other 400s are bad requests");

        HttpResponse none;
        none.networkError = "Could not connect";
        auto network = openai::parseResponse(none);
        check(network.error->kind == ErrorKind::network && network.error->retryable(), "no reply is a retryable network error");

        HttpResponse cancelled;
        cancelled.cancelled = true;
        check(openai::parseResponse(cancelled).stopReason == StopReason::cancelled, "a cancelled request is a cancelled response");

        check(! openai::parseResponse(ok("not json")).ok(), "a reply that is not JSON is an error");
        check(! openai::parseResponse(ok(R"({"choices":[]})")).ok(), "a reply with no choices is an error");
    }

    // ---- The provider ----
    {
        auto http = std::make_shared<FakeHttp>();
        http->replies.push_back(ok(R"({"choices":[{"message":{"content":"Hi there"},"finish_reason":"stop"}]})"));

        OpenAiProvider::Settings settings;
        settings.baseUrl = "https://example.test/v1";
        settings.apiKey = "sk-secret";
        settings.model = "configured-model";
        OpenAiProvider provider(settings, http);

        ChatRequest request;
        request.messages.push_back(Message::text(Role::user, "hi"));
        request.tools.push_back({ "t", "T", R"({"type":"object"})" });
        CancelToken cancel;
        std::string streamed;
        auto response = provider.complete(request, cancel, [&](const StreamEvent& e) { if (e.kind == StreamEvent::Kind::textDelta) streamed += e.text; });

        check(response.ok() && response.assistant.plainText() == "Hi there", "the provider returns the reply");
        check(http->sent.size() == 1 && http->sent[0].url == "https://example.test/v1/chat/completions", "it posts to base URL and path");
        check(headerValue(http->sent[0], "Authorization") == "Bearer sk-secret", "the key signs the request");
        check(contains(http->sent[0].body, "configured-model"), "the configured model is used when the request names none");
        check(streamed == "Hi there", "the reply is delivered to a stream handler");

        // No key: no Authorization header (local servers).
        auto localHttp = std::make_shared<FakeHttp>();
        localHttp->replies.push_back(ok(R"({"choices":[{"message":{"content":"x"},"finish_reason":"stop"}]})"));
        OpenAiProvider::Settings local;
        local.model = "m";
        OpenAiProvider localProvider(local, localHttp);
        localProvider.complete(request, cancel, nullptr);
        check(headerValue(localHttp->sent[0], "Authorization").empty(), "no key means no Authorization header");

        // A model that cannot call tools is not sent any.
        auto noToolsHttp = std::make_shared<FakeHttp>();
        noToolsHttp->replies.push_back(ok(R"({"choices":[{"message":{"content":"x"},"finish_reason":"stop"}]})"));
        OpenAiProvider::Settings noTools;
        noTools.model = "m";
        noTools.capabilities.toolCalling = false;
        OpenAiProvider noToolsProvider(noTools, noToolsHttp);
        noToolsProvider.complete(request, cancel, nullptr);
        check(! contains(noToolsHttp->sent[0].body, "\"tools\""), "tools are not sent when the model cannot call them");

        CancelToken already;
        already.cancel();
        auto cancelledResponse = provider.complete(request, already, nullptr);
        check(cancelledResponse.stopReason == StopReason::cancelled && http->sent.size() == 1, "an already-cancelled request is not sent");
    }

    // ---- The provider inside the agent loop ----
    {
        auto http = std::make_shared<FakeHttp>();
        http->replies.push_back(ok(R"({"choices":[{"message":{"content":null,"tool_calls":[{"id":"call_1","type":"function",
                                     "function":{"name":"add_track","arguments":"{\"name\":\"Bass\"}"}}]},"finish_reason":"tool_calls"}],
                                     "usage":{"prompt_tokens":50,"completion_tokens":10}})"));
        http->replies.push_back(ok(R"({"choices":[{"message":{"content":"Added Bass."},"finish_reason":"stop"}],
                                     "usage":{"prompt_tokens":80,"completion_tokens":5}})"));

        OpenAiProvider::Settings settings;
        settings.model = "m";
        OpenAiProvider provider(settings, http);

        std::string addedName;
        ToolRegistry tools;
        tools.add(std::make_shared<FunctionTool>(
            ToolSpec { "add_track", "Add a track", R"({"type":"object","properties":{"name":{"type":"string"}},"required":["name"]})" }, Effect::write,
            [&](const std::string& args, ToolContext&)
            {
                const auto parsedArgs = parse(args);
                auto* object = parsedArgs.getDynamicObject();
                addedName = object->getProperty("name").toString().toStdString();
                return ToolResult::success("Track 1 added.");
            }));

        AgentEngine engine(provider, tools);
        CancelToken cancel;
        AgentRequest request;
        request.model = "m";
        request.userMessage = "Add a track called Bass.";
        auto result = engine.run(request, cancel);

        check(result.reason == EndReason::finished && result.finalText == "Added Bass." && addedName == "Bass",
              "the engine, the OpenAI provider and a tool complete a request together");
        check(result.usage.inputTokens == 130 && result.usage.outputTokens == 15, "usage adds up across the steps");

        // The second HTTP request carries the tool result the way OpenAI expects it.
        const auto secondBody = parse(http->sent[1].body);
        auto* second = secondBody.getDynamicObject();
        const auto secondMessages = second->getProperty("messages");
        const auto* messages = secondMessages.getArray();
        auto* last = (*messages)[messages->size() - 1].getDynamicObject();
        check(last->getProperty("role").toString() == "tool" && last->getProperty("tool_call_id").toString() == "call_1"
                  && last->getProperty("content").toString() == "Track 1 added.",
              "the second request carries the tool's result under its call id");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
