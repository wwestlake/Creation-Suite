#pragma once

#include "Provider.h"

#include <mutex>
#include <string>
#include <vector>

namespace creation::ai
{
// A provider that answers from a script, for tests. Each call to complete() returns the next scripted
// response, and every request it receives is kept so a test can check exactly what the engine sent (the
// conversation, the tools offered, the system prompt). Deterministic: no network, no randomness.
class ScriptedProvider final : public Provider
{
public:
    explicit ScriptedProvider(ProviderCapabilities capabilitiesToReport = { true, true, true, 0 });

    // Queue what the model will say next.
    void addTextReply(std::string text);
    void addToolCallReply(std::string callId, std::string toolName, std::string argumentsJson, std::string leadInText = {});
    void addResponse(ChatResponse response);
    void addError(ErrorKind kind, std::string message, double retryAfterSeconds = 0.0);

    std::string name() const override { return "scripted"; }
    ProviderCapabilities capabilities() const override { return reported; }
    ChatResponse complete(const ChatRequest& request, const CancelToken& cancel, const StreamHandler& onStream) override;

    // What the engine sent, oldest first.
    std::vector<ChatRequest> receivedRequests() const;
    size_t repliesRemaining() const;

private:
    ProviderCapabilities reported;
    mutable std::mutex mutex;
    std::vector<ChatResponse> replies;
    size_t next = 0;
    std::vector<ChatRequest> received;
};
}
