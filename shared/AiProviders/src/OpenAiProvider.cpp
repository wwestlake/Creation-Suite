#include "creation/ai/OpenAiProvider.h"

namespace creation::ai
{
OpenAiProvider::OpenAiProvider(Settings settingsToUse, std::shared_ptr<HttpClient> clientToUse)
    : settings(std::move(settingsToUse)), client(std::move(clientToUse))
{
}

ChatResponse OpenAiProvider::complete(const ChatRequest& request, const CancelToken& cancel, const StreamHandler& onStream)
{
    ChatRequest toSend = request;
    if (toSend.model.empty())
        toSend.model = settings.model;
    if (! settings.capabilities.toolCalling)
        toSend.tools.clear();

    HttpRequest http;
    http.url = settings.baseUrl + settings.chatPath;
    http.headers.push_back({ "Content-Type", "application/json" });
    http.headers.push_back({ "Accept", "application/json" });
    if (! settings.apiKey.empty())
        http.headers.push_back({ "Authorization", "Bearer " + settings.apiKey });
    http.body = openai::buildRequestBody(toSend, settings.wire);
    http.timeoutSeconds = settings.timeoutSeconds;

    if (cancel.isCancelled())
    {
        ChatResponse cancelled;
        cancelled.stopReason = StopReason::cancelled;
        cancelled.error = ProviderError { ErrorKind::cancelled, "Cancelled.", 0, 0.0 };
        return cancelled;
    }

    auto response = openai::parseResponse(client->post(http, cancel));

    // This transport is not streaming: deliver what arrived as it would have streamed, so a host that shows
    // the reply as it forms works the same with every provider.
    if (response.ok() && onStream)
    {
        for (const auto& block : response.assistant.content)
        {
            if (const auto* text = std::get_if<TextBlock>(&block))
            {
                onStream({ StreamEvent::Kind::textDelta, text->text, {}, {} });
            }
            else if (const auto* call = std::get_if<ToolCallBlock>(&block))
            {
                onStream({ StreamEvent::Kind::toolCallStarted, {}, call->id, call->name });
                onStream({ StreamEvent::Kind::toolCallDelta, call->argumentsJson, call->id, call->name });
                onStream({ StreamEvent::Kind::toolCallFinished, {}, call->id, call->name });
            }
        }
    }
    return response;
}
}
