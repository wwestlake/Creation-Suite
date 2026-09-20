#include "creation/ai/AgentEngine.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace creation::ai
{
namespace
{
using Clock = std::chrono::steady_clock;

size_t sizeOfMessage(const Message& message)
{
    size_t size = 16;
    for (const auto& block : message.content)
    {
        if (const auto* text = std::get_if<TextBlock>(&block))
            size += text->text.size();
        else if (const auto* call = std::get_if<ToolCallBlock>(&block))
            size += call->name.size() + call->argumentsJson.size() + call->id.size();
        else if (const auto* result = std::get_if<ToolResultBlock>(&block))
            size += result->content.size() + result->callId.size();
    }
    return size;
}

bool hasToolCalls(const Message& message)
{
    for (const auto& block : message.content)
        if (std::holds_alternative<ToolCallBlock>(block))
            return true;
    return false;
}

// Sleeps up to `seconds`, returning early (false) if the run is cancelled or over time.
bool waitInterruptibly(double seconds, const CancelToken& cancel, Clock::time_point deadline)
{
    const auto until = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
    while (Clock::now() < until)
    {
        if (cancel.isCancelled() || Clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

std::string signatureOf(const ToolCallBlock& call)
{
    return call.name + "\n" + call.argumentsJson;
}
}

AgentEngine::AgentEngine(Provider& providerToUse, ToolRegistry& toolsToUse)
    : provider(providerToUse), tools(toolsToUse)
{
}

size_t AgentEngine::sizeOf(const std::vector<Message>& messages)
{
    size_t total = 0;
    for (const auto& message : messages)
        total += sizeOfMessage(message);
    return total;
}

int AgentEngine::trimToFit(std::vector<Message>& messages, size_t maxChars)
{
    if (messages.size() < 3 || sizeOf(messages) <= maxChars)
        return 0;

    // A unit is one message, or an assistant message with tool calls together with the tool message that
    // answers it: they are dropped or kept as a pair.
    struct Unit { size_t begin; size_t count; };
    std::vector<Unit> units;
    for (size_t i = 1; i < messages.size();)
    {
        if (hasToolCalls(messages[i]) && i + 1 < messages.size() && messages[i + 1].role == Role::tool)
        {
            units.push_back({ i, 2 });
            i += 2;
        }
        else
        {
            units.push_back({ i, 1 });
            ++i;
        }
    }

    // Drop oldest units (after the first message) while too big, always keeping the most recent unit.
    size_t drop = 0;
    size_t removed = 0;
    size_t total = sizeOf(messages);
    while (drop + 1 < units.size() && total > maxChars)
    {
        for (size_t k = 0; k < units[drop].count; ++k)
            total -= sizeOfMessage(messages[units[drop].begin + k]);
        removed += units[drop].count;
        ++drop;
    }
    if (removed == 0)
        return 0;

    std::vector<Message> kept;
    kept.push_back(messages[0]);
    kept.push_back(Message::text(Role::user, "[" + std::to_string(removed) + " earlier messages were removed to fit the context window.]"));
    for (size_t u = drop; u < units.size(); ++u)
        for (size_t k = 0; k < units[u].count; ++k)
            kept.push_back(messages[units[u].begin + k]);
    messages = std::move(kept);
    return (int) removed;
}

AgentResult AgentEngine::run(const AgentRequest& request, const CancelToken& cancel)
{
    AgentResult result;
    const auto& limits = request.limits;
    const auto started = Clock::now();
    const auto deadline = started + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(limits.maxSeconds));
    const auto capabilities = provider.capabilities();

    std::vector<Message> conversation = request.history;
    auto userMessage = Message::text(Role::user, request.userMessage);
    conversation.push_back(userMessage);
    result.messages.push_back(userMessage);

    auto append = [&](const Message& message)
    {
        conversation.push_back(message);
        result.messages.push_back(message);
    };

    auto end = [&](EndReason reason, std::string detail)
    {
        result.reason = reason;
        result.detail = std::move(detail);
        return result;
    };

    auto notice = [&](const std::string& text)
    {
        if (request.events.onNotice)
            request.events.onNotice(text);
    };

    const bool canUseTools = capabilities.toolCalling;
    if (! canUseTools && ! tools.specs().empty())
        notice("This model cannot call tools, so none are offered.");

    std::string lastSignature;
    int repeatCount = 0;
    int loopRefusals = 0;
    int failedStepsInARow = 0;
    int continuations = 0;

    for (int step = 1; step <= limits.maxSteps; ++step)
    {
        if (cancel.isCancelled())
            return end(EndReason::stopped, "Stopped.");
        if (Clock::now() > deadline)
            return end(EndReason::timeLimit, "The run used its " + std::to_string((int) limits.maxSeconds) + " second limit.");
        if (limits.maxTotalTokens > 0 && result.usage.inputTokens + result.usage.outputTokens >= limits.maxTotalTokens)
            return end(EndReason::tokenLimit, "The run used its token limit.");

        if (request.events.onStepStarted)
            request.events.onStepStarted(step);

        if (capabilities.contextWindowTokens > 0)
        {
            const auto budget = (size_t) (capabilities.contextWindowTokens * 4.0 * limits.contextBudgetFraction);
            const int removed = trimToFit(conversation, budget);
            if (removed > 0)
                notice(std::to_string(removed) + " earlier messages were dropped to fit the model's context window.");
        }

        ChatRequest chat;
        chat.model = request.model;
        chat.system = request.system;
        chat.messages = conversation;
        chat.temperature = request.temperature;
        chat.maxOutputTokens = request.maxOutputTokens;
        if (canUseTools)
            chat.tools = tools.specs();

        StreamHandler onStream;
        if (request.events.onText)
        {
            onStream = [&](const StreamEvent& event)
            {
                if (event.kind == StreamEvent::Kind::textDelta)
                    request.events.onText(event.text);
            };
        }

        // Ask the model, retrying errors that are worth retrying.
        ChatResponse response;
        for (int attempt = 0;; ++attempt)
        {
            response = provider.complete(chat, cancel, onStream);
            if (response.ok() || ! response.error->retryable() || attempt >= limits.maxProviderRetries)
                break;

            const double delay = response.error->retryAfterSeconds > 0.0
                                     ? std::min(response.error->retryAfterSeconds, limits.retryMaxDelaySeconds)
                                     : std::min(limits.retryMaxDelaySeconds, limits.retryBaseDelaySeconds * std::pow(2.0, attempt));
            notice("The provider had a problem (" + response.error->message + "). Trying again in "
                   + std::to_string(delay) + " seconds.");
            if (! waitInterruptibly(delay, cancel, deadline))
            {
                response = ChatResponse {};
                response.stopReason = cancel.isCancelled() ? StopReason::cancelled : StopReason::error;
                response.error = ProviderError { cancel.isCancelled() ? ErrorKind::cancelled : ErrorKind::network, "Stopped while waiting to retry.", 0, 0.0 };
                break;
            }
        }

        result.usage += response.usage;
        ++result.steps;

        if (! response.ok())
        {
            if (response.error->kind == ErrorKind::cancelled)
                return end(EndReason::stopped, "Stopped.");
            if (response.error->kind == ErrorKind::unsupported)
                return end(EndReason::unsupported, response.error->message);
            return end(EndReason::providerError, response.error->message);
        }

        append(response.assistant);
        const auto calls = response.assistant.toolCalls();

        if (calls.empty())
        {
            if (response.stopReason == StopReason::maxTokens && continuations < limits.maxContinuations)
            {
                ++continuations;
                append(Message::text(Role::user, "Continue."));
                continue;
            }
            result.finalText = response.assistant.plainText();
            return end(EndReason::finished, {});
        }

        // The model asked for tools. Every call gets a result, even a refused one, so the conversation stays valid.
        Message resultsMessage;
        resultsMessage.role = Role::tool;
        bool anySucceeded = false;
        bool stoppedDuringTools = false;
        bool endForLoop = false;

        for (size_t i = 0; i < calls.size(); ++i)
        {
            const auto& call = calls[i];
            ToolResult toolResult;

            if (stoppedDuringTools || cancel.isCancelled())
            {
                stoppedDuringTools = true;
                toolResult = ToolResult::failure("The request was stopped before this call ran.");
            }
            else if ((int) i >= limits.maxToolCallsPerStep)
            {
                toolResult = ToolResult::failure("Too many tool calls in one reply (the limit is " + std::to_string(limits.maxToolCallsPerStep)
                                                 + "). Make fewer calls per step.");
            }
            else
            {
                const auto signature = signatureOf(call);
                repeatCount = (signature == lastSignature) ? repeatCount + 1 : 1;
                lastSignature = signature;

                if (repeatCount > limits.maxRepeatedIdenticalCalls)
                {
                    ++loopRefusals;
                    toolResult = ToolResult::failure("You have already made this exact call " + std::to_string(repeatCount - 1)
                                                     + " times in a row. Repeating it will not change the outcome. Try a different approach, "
                                                       "or tell the user what is blocking you.");
                    notice("Refused a repeated identical call to " + call.name + ".");
                    if (loopRefusals >= 2)
                        endForLoop = true;
                }
                else
                {
                    const auto tool = tools.find(call.name);
                    ToolDecision decision;
                    if (tool != nullptr && request.policy)
                        decision = request.policy(*tool, call);

                    if (request.events.onToolCall)
                        request.events.onToolCall(call);

                    if (! decision.allow)
                        toolResult = ToolResult::failure("Not allowed: " + (decision.reason.empty() ? std::string("this action was declined.") : decision.reason));
                    else
                        toolResult = tools.execute(call, cancel);
                    ++result.toolCalls;

                    if (request.events.onToolResult)
                        request.events.onToolResult(call, toolResult);
                }
            }

            anySucceeded = anySucceeded || toolResult.ok;
            resultsMessage.content.push_back(ToolResultBlock { call.id, toolResult.content, ! toolResult.ok });
        }
        append(resultsMessage);

        if (stoppedDuringTools || cancel.isCancelled())
            return end(EndReason::stopped, "Stopped.");
        if (endForLoop)
            return end(EndReason::loopDetected, "The model kept repeating the same call. Try rephrasing the request.");

        failedStepsInARow = anySucceeded ? 0 : failedStepsInARow + 1;
        if (failedStepsInARow >= limits.maxConsecutiveFailedSteps)
            return end(EndReason::tooManyFailures, "Every tool call failed for " + std::to_string(failedStepsInARow) + " steps in a row.");
    }

    return end(EndReason::stepLimit, "The run used its " + std::to_string(limits.maxSteps) + " step limit.");
}
}
