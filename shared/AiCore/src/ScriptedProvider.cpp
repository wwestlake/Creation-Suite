#include "creation/ai/ScriptedProvider.h"

namespace creation::ai
{
ScriptedProvider::ScriptedProvider(ProviderCapabilities capabilitiesToReport)
    : reported(capabilitiesToReport)
{
}

void ScriptedProvider::addTextReply(std::string text)
{
    ChatResponse response;
    response.assistant = Message::text(Role::assistant, std::move(text));
    response.stopReason = StopReason::endOfTurn;
    addResponse(std::move(response));
}

void ScriptedProvider::addToolCallReply(std::string callId, std::string toolName, std::string argumentsJson, std::string leadInText)
{
    ChatResponse response;
    response.assistant.role = Role::assistant;
    if (! leadInText.empty())
        response.assistant.content.push_back(TextBlock { std::move(leadInText) });
    response.assistant.content.push_back(ToolCallBlock { std::move(callId), std::move(toolName), std::move(argumentsJson) });
    response.stopReason = StopReason::toolUse;
    addResponse(std::move(response));
}

void ScriptedProvider::addResponse(ChatResponse response)
{
    std::lock_guard<std::mutex> lock(mutex);
    replies.push_back(std::move(response));
}

void ScriptedProvider::addError(ErrorKind kind, std::string message, double retryAfterSeconds)
{
    ChatResponse response;
    response.stopReason = StopReason::error;
    response.error = ProviderError { kind, std::move(message), 0, retryAfterSeconds };
    addResponse(std::move(response));
}

ChatResponse ScriptedProvider::complete(const ChatRequest& request, const CancelToken& cancel, const StreamHandler& onStream)
{
    ChatResponse response;
    {
        std::lock_guard<std::mutex> lock(mutex);
        received.push_back(request);

        if (cancel.isCancelled())
        {
            response.stopReason = StopReason::cancelled;
            response.error = ProviderError { ErrorKind::cancelled, "Cancelled.", 0, 0.0 };
            return response;
        }

        if (next >= replies.size())
        {
            response.stopReason = StopReason::error;
            response.error = ProviderError { ErrorKind::badRequest, "The scripted provider has no more replies.", 0, 0.0 };
            return response;
        }
        response = replies[next++];
    }

    // Stream what a real provider would: the text, then each tool call.
    if (onStream)
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

std::vector<ChatRequest> ScriptedProvider::receivedRequests() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return received;
}

size_t ScriptedProvider::repliesRemaining() const
{
    std::lock_guard<std::mutex> lock(mutex);
    return replies.size() - next;
}
}
