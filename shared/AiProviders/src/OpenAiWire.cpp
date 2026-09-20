#include "creation/ai/OpenAiWire.h"

#include <juce_core/juce_core.h>

namespace creation::ai::openai
{
namespace
{
juce::var makeMessage(const char* role, const juce::var& content)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("role", role);
    object->setProperty("content", content);
    return juce::var(object);
}

const char* roleName(Role role)
{
    switch (role)
    {
        case Role::system: return "system";
        case Role::user: return "user";
        case Role::assistant: return "assistant";
        case Role::tool: return "tool";
    }
    return "user";
}

juce::var parseOrNull(const std::string& json)
{
    juce::var parsed;
    if (juce::JSON::parse(juce::String(json), parsed).failed())
        return {};
    return parsed;
}

ChatResponse errorResponse(ErrorKind kind, std::string message, int status = 0, double retryAfter = 0.0)
{
    ChatResponse response;
    response.stopReason = kind == ErrorKind::cancelled ? StopReason::cancelled : StopReason::error;
    response.error = ProviderError { kind, std::move(message), status, retryAfter };
    return response;
}
}

std::string buildRequestBody(const ChatRequest& request, const WireOptions& options)
{
    juce::Array<juce::var> messages;

    if (! request.system.empty())
        messages.add(makeMessage("system", juce::String(request.system)));

    for (const auto& message : request.messages)
    {
        switch (message.role)
        {
            case Role::system:
            case Role::user:
                messages.add(makeMessage(roleName(message.role), juce::String(message.plainText())));
                break;

            case Role::assistant:
            {
                auto text = message.plainText();
                juce::var entry = makeMessage("assistant", text.empty() ? juce::var() : juce::var(juce::String(text)));
                juce::Array<juce::var> calls;
                for (const auto& call : message.toolCalls())
                {
                    auto* function = new juce::DynamicObject();
                    function->setProperty("name", juce::String(call.name));
                    function->setProperty("arguments", juce::String(call.argumentsJson.empty() ? "{}" : call.argumentsJson));
                    auto* callObject = new juce::DynamicObject();
                    callObject->setProperty("id", juce::String(call.id));
                    callObject->setProperty("type", "function");
                    callObject->setProperty("function", juce::var(function));
                    calls.add(juce::var(callObject));
                }
                if (! calls.isEmpty())
                    entry.getDynamicObject()->setProperty("tool_calls", juce::var(calls));
                messages.add(entry);
                break;
            }

            case Role::tool:
                // One wire message per result, each quoting its call id.
                for (const auto& block : message.content)
                {
                    if (const auto* result = std::get_if<ToolResultBlock>(&block))
                    {
                        auto entry = makeMessage("tool", juce::String(result->content));
                        entry.getDynamicObject()->setProperty("tool_call_id", juce::String(result->callId));
                        messages.add(entry);
                    }
                }
                break;
        }
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("model", juce::String(request.model));
    root->setProperty("messages", juce::var(messages));
    if (options.sendTemperature)
        root->setProperty("temperature", request.temperature);
    root->setProperty(options.useMaxCompletionTokens ? "max_completion_tokens" : "max_tokens", request.maxOutputTokens);

    if (! request.tools.empty())
    {
        juce::Array<juce::var> tools;
        for (const auto& tool : request.tools)
        {
            auto* function = new juce::DynamicObject();
            function->setProperty("name", juce::String(tool.name));
            function->setProperty("description", juce::String(tool.description));
            auto schema = parseOrNull(tool.parametersSchemaJson);
            function->setProperty("parameters", schema.isVoid() ? juce::var(new juce::DynamicObject()) : schema);
            auto* entry = new juce::DynamicObject();
            entry->setProperty("type", "function");
            entry->setProperty("function", juce::var(function));
            tools.add(juce::var(entry));
        }
        root->setProperty("tools", juce::var(tools));
    }

    return juce::JSON::toString(juce::var(root), true).toStdString();
}

ChatResponse parseResponse(const HttpResponse& http)
{
    if (http.cancelled)
        return errorResponse(ErrorKind::cancelled, "Cancelled.");

    if (http.status == 0)
        return errorResponse(ErrorKind::network, http.networkError.empty() ? "Could not reach the AI provider." : http.networkError);

    const auto parsed = parseOrNull(http.body);

    if (http.status < 200 || http.status >= 300)
    {
        std::string message = "The AI provider returned HTTP " + std::to_string(http.status) + ".";
        std::string code;
        if (auto* root = parsed.getDynamicObject())
        {
            if (auto* error = root->getProperty("error").getDynamicObject())
            {
                const auto text = error->getProperty("message").toString().toStdString();
                if (! text.empty())
                    message = text;
                code = error->getProperty("code").toString().toStdString();
            }
        }

        double retryAfter = 0.0;
        if (! http.retryAfter.empty())
            retryAfter = juce::String(http.retryAfter).getDoubleValue();

        ErrorKind kind = ErrorKind::badRequest;
        if (http.status == 401 || http.status == 403)
            kind = ErrorKind::authentication;
        else if (http.status == 429)
            kind = ErrorKind::rateLimited;
        else if (http.status >= 500)
            kind = ErrorKind::serverError;
        else if (code == "context_length_exceeded" || message.find("maximum context length") != std::string::npos)
            kind = ErrorKind::contextTooLong;

        return errorResponse(kind, message, http.status, retryAfter);
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
        return errorResponse(ErrorKind::badRequest, "The AI provider returned something that is not JSON.", http.status);

    const auto choices = root->getProperty("choices");
    if (! choices.isArray() || choices.getArray()->isEmpty())
        return errorResponse(ErrorKind::badRequest, "The AI provider's reply had no choices.", http.status);

    auto* choice = (*choices.getArray())[0].getDynamicObject();
    auto* message = choice != nullptr ? choice->getProperty("message").getDynamicObject() : nullptr;
    if (message == nullptr)
        return errorResponse(ErrorKind::badRequest, "The AI provider's reply had no message.", http.status);

    ChatResponse response;
    response.assistant.role = Role::assistant;

    const auto content = message->getProperty("content");
    if (content.isString() && content.toString().isNotEmpty())
        response.assistant.content.push_back(TextBlock { content.toString().toStdString() });

    const auto calls = message->getProperty("tool_calls");
    if (calls.isArray())
    {
        for (const auto& call : *calls.getArray())
        {
            auto* callObject = call.getDynamicObject();
            auto* function = callObject != nullptr ? callObject->getProperty("function").getDynamicObject() : nullptr;
            if (function == nullptr)
                continue;
            response.assistant.content.push_back(ToolCallBlock {
                callObject->getProperty("id").toString().toStdString(),
                function->getProperty("name").toString().toStdString(),
                function->getProperty("arguments").toString().toStdString() });
        }
    }

    const auto finish = choice->getProperty("finish_reason").toString();
    if (! response.assistant.toolCalls().empty() || finish == "tool_calls")
        response.stopReason = StopReason::toolUse;
    else if (finish == "length")
        response.stopReason = StopReason::maxTokens;
    else
        response.stopReason = StopReason::endOfTurn;

    if (auto* usage = root->getProperty("usage").getDynamicObject())
    {
        response.usage.inputTokens = (juce::int64) usage->getProperty("prompt_tokens");
        response.usage.outputTokens = (juce::int64) usage->getProperty("completion_tokens");
    }
    return response;
}
}
