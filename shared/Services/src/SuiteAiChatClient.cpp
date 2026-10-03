#include <creation/services/SuiteAiChatClient.h>
#include <creation/services/SuiteAiProviderRuntime.h>

namespace
{
juce::var makeMessage(const juce::String& role, const juce::String& content)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("role", role);
    object->setProperty("content", content);
    return juce::var(object);
}
}

namespace creation::services
{

bool SuiteAiChatClient::sendChatCompletion(const SuiteAiResolvedRuntimeSettings& settings,
                                           const juce::String& systemPrompt,
                                           const juce::String& userPrompt,
                                           ChatResult& result) const
{
    result = {};

    const auto profile = SuiteAiProviderRuntime::resolveProfile(settings.providerId);

    if (SuiteAiProviderRuntime::requiresApiKey(profile, settings.apiKey))
    {
        result.errorMessage = "Enter your provider API key in Settings.";
        return false;
    }

    auto model = settings.modelName.trim();
    if (model.isEmpty())
        model = SuiteAiProviderRuntime::defaultModelName(profile);

    juce::Array<juce::var> messages;
    if (systemPrompt.isNotEmpty())
        messages.add(makeMessage("system", systemPrompt));
    messages.add(makeMessage("user", userPrompt));

    auto* root = new juce::DynamicObject();
    root->setProperty("model", model);
    root->setProperty("temperature", 0.4);
    root->setProperty("messages", juce::var(messages));

    auto urlPath = profile.chatCompletionsPath;
    auto headers = SuiteAiProviderRuntime::buildAuthHeaders(profile, settings.apiKey);
    if (profile.isOllamaStyle())
    {
        root->setProperty("stream", false);
    }

    auto body = juce::JSON::toString(juce::var(root), false);

    auto url = juce::URL(SuiteAiProviderRuntime::normalizeBaseUrl(settings.baseUrl, profile) + urlPath)
                   .withPOSTData(body);
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(30000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(headers));

    if (stream == nullptr)
    {
        result.errorMessage = "Could not connect to the selected AI provider.";
        return false;
    }

    result.statusCode = statusCode;
    result.rawResponse = stream->readEntireStreamAsString();

    if (statusCode < 200 || statusCode >= 300)
    {
        result.errorMessage = profile.displayName + " error (HTTP " + juce::String(statusCode) + "): "
                              + result.rawResponse.substring(0, 300);
        return false;
    }

    auto parsed = juce::JSON::parse(result.rawResponse);
    if (! parsed.isObject())
    {
        result.errorMessage = profile.displayName + " returned invalid JSON.";
        return false;
    }

    auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        result.errorMessage = profile.displayName + " response was missing its root object.";
        return false;
    }

    if (profile.isOllamaStyle())
    {
        auto message = object->getProperty("message");
        auto* messageObject = message.getDynamicObject();
        if (messageObject == nullptr)
        {
            result.errorMessage = profile.displayName + " response was missing its message object.";
            return false;
        }

        result.text = messageObject->getProperty("content").toString().trim();
    }
    else
    {
        auto choices = object->getProperty("choices");
        if (! choices.isArray() || choices.getArray() == nullptr || choices.getArray()->size() == 0)
        {
            result.errorMessage = profile.displayName + " response did not include any choices.";
            return false;
        }

        auto firstChoice = choices.getArray()->getReference(0);
        auto* choiceObject = firstChoice.getDynamicObject();
        if (choiceObject == nullptr)
        {
            result.errorMessage = profile.displayName + " response choice was malformed.";
            return false;
        }

        auto message = choiceObject->getProperty("message");
        auto* messageObject = message.getDynamicObject();
        if (messageObject == nullptr)
        {
            result.errorMessage = profile.displayName + " response choice did not include a message.";
            return false;
        }

        result.text = messageObject->getProperty("content").toString().trim();
    }

    if (result.text.isEmpty())
        result.text = "(The model returned an empty response.)";

    return true;
}

bool SuiteAiChatClient::supportsToolCalling(const SuiteAiResolvedRuntimeSettings& settings)
{
    const auto profile = SuiteAiProviderRuntime::resolveProfile(settings.providerId);
    return profile.supportsOpenAiChatStyle && ! profile.isOllamaStyle();
}

bool SuiteAiChatClient::sendTurn(const SuiteAiResolvedRuntimeSettings& settings,
                                 const std::vector<Message>& messages,
                                 const std::vector<ToolSpec>& tools,
                                 TurnResult& result) const
{
    result = {};
    const auto profile = SuiteAiProviderRuntime::resolveProfile(settings.providerId);
    if (! supportsToolCalling(settings))
    {
        result.errorMessage = profile.displayName + " cannot be sent tools by the suite yet.";
        return false;
    }
    if (SuiteAiProviderRuntime::requiresApiKey(profile, settings.apiKey))
    {
        result.errorMessage = "Enter your provider API key in Settings.";
        return false;
    }
    auto model = settings.modelName.trim();
    if (model.isEmpty())
        model = SuiteAiProviderRuntime::defaultModelName(profile);

    juce::Array<juce::var> wireMessages;
    for (const auto& message : messages)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty("role", message.role);
        if (message.role == "tool")
            m->setProperty("tool_call_id", message.toolCallId);
        if (! message.toolCalls.empty())
        {
            juce::Array<juce::var> calls;
            for (const auto& call : message.toolCalls)
            {
                auto* function = new juce::DynamicObject();
                function->setProperty("name", call.name);
                function->setProperty("arguments", call.arguments);
                auto* c = new juce::DynamicObject();
                c->setProperty("id", call.id);
                c->setProperty("type", "function");
                c->setProperty("function", juce::var(function));
                calls.add(juce::var(c));
            }
            m->setProperty("tool_calls", juce::var(calls));
            // An assistant turn that only calls tools has no text.
            m->setProperty("content", message.content.isNotEmpty() ? juce::var(message.content) : juce::var());
        }
        else
            m->setProperty("content", message.content);
        wireMessages.add(juce::var(m));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty("model", model);
    root->setProperty("temperature", 0.2);
    root->setProperty("messages", juce::var(wireMessages));
    if (! tools.empty())
    {
        juce::Array<juce::var> wireTools;
        for (const auto& tool : tools)
        {
            auto* function = new juce::DynamicObject();
            function->setProperty("name", tool.name);
            function->setProperty("description", tool.description);
            function->setProperty("parameters", tool.parameters);
            auto* t = new juce::DynamicObject();
            t->setProperty("type", "function");
            t->setProperty("function", juce::var(function));
            wireTools.add(juce::var(t));
        }
        root->setProperty("tools", juce::var(wireTools));
    }

    auto url = juce::URL(SuiteAiProviderRuntime::normalizeBaseUrl(settings.baseUrl, profile) + profile.chatCompletionsPath)
                   .withPOSTData(juce::JSON::toString(juce::var(root), true));
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(60000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(SuiteAiProviderRuntime::buildAuthHeaders(profile, settings.apiKey)));
    result.statusCode = statusCode;
    if (stream == nullptr)
    {
        result.errorMessage = "Could not connect to the selected AI provider.";
        return false;
    }
    const auto raw = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
    {
        result.errorMessage = profile.displayName + " error (HTTP " + juce::String(statusCode) + "): " + raw.substring(0, 300);
        return false;
    }

    const auto parsed = juce::JSON::parse(raw);
    const auto* choices = parsed.getProperty("choices", {}).getArray();
    if (choices == nullptr || choices->isEmpty())
    {
        result.errorMessage = profile.displayName + " response did not include any choices.";
        return false;
    }
    const auto message = choices->getReference(0).getProperty("message", {});
    if (! message.isObject())
    {
        result.errorMessage = profile.displayName + " response choice did not include a message.";
        return false;
    }
    const auto content = message.getProperty("content", {});
    result.text = content.isString() ? content.toString().trim() : juce::String();
    if (const auto* calls = message.getProperty("tool_calls", {}).getArray())
        for (const auto& call : *calls)
        {
            const auto function = call.getProperty("function", {});
            result.toolCalls.push_back({ call.getProperty("id", {}).toString(), function.getProperty("name", {}).toString(),
                                         function.getProperty("arguments", {}).toString() });
        }
    if (result.text.isEmpty() && result.toolCalls.empty())
    {
        result.errorMessage = profile.displayName + " returned neither a reply nor a tool call.";
        return false;
    }
    return true;
}

}
