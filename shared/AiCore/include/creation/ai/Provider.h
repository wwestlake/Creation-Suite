#pragma once

#include "Messages.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

namespace creation::ai
{
// Cooperative cancellation. The engine's Stop button sets it; a provider checks it while it waits on the
// network and returns a cancelled response instead of finishing.
class CancelToken
{
public:
    void cancel() noexcept { cancelled.store(true); }
    bool isCancelled() const noexcept { return cancelled.load(); }
    void reset() noexcept { cancelled.store(false); }

private:
    std::atomic<bool> cancelled { false };
};

// What arrives while a model is answering, for hosts that show the answer as it forms.
struct StreamEvent
{
    enum class Kind
    {
        textDelta,       // more of the reply text
        toolCallStarted, // the model began a tool call (name and id known)
        toolCallDelta,   // more of that call's JSON arguments
        toolCallFinished
    };

    Kind kind = Kind::textDelta;
    std::string text;         // textDelta: the new text; toolCallDelta: the new JSON fragment
    std::string toolCallId;
    std::string toolName;
};

using StreamHandler = std::function<void(const StreamEvent&)>;

// What a provider and model combination can do. The engine adapts to it (for example it will not offer tools
// to a model that cannot call them).
struct ProviderCapabilities
{
    bool toolCalling = false;
    bool streaming = false;
    bool systemPrompt = true;
    int contextWindowTokens = 0;   // 0 = unknown
};

// One AI provider, speaking that provider's own protocol. complete() blocks (call it off the message
// thread), honours the cancel token, reports failures as data in the response rather than throwing, and
// never logs or stores a key. `onStream` may be null.
class Provider
{
public:
    virtual ~Provider() = default;

    virtual std::string name() const = 0;
    virtual ProviderCapabilities capabilities() const = 0;
    virtual ChatResponse complete(const ChatRequest& request, const CancelToken& cancel, const StreamHandler& onStream) = 0;
};
}
