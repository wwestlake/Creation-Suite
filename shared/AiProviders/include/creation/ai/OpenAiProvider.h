#pragma once

#include "OpenAiWire.h"

#include <creation/ai/Provider.h>

#include <memory>
#include <string>

namespace creation::ai
{
// A provider that speaks the OpenAI chat-completions protocol. Built from plain settings (where, with which
// key, which model) so it depends on no application code; the suite's account settings are turned into these
// by the caller. The key is used only to sign requests and is never logged or returned.
class OpenAiProvider final : public Provider
{
public:
    struct Settings
    {
        std::string displayName = "OpenAI-compatible";
        std::string baseUrl = "https://api.openai.com/v1";   // without a trailing slash
        std::string chatPath = "/chat/completions";
        std::string apiKey;                                   // empty: no Authorization header (local servers)
        std::string model;
        openai::WireOptions wire;
        ProviderCapabilities capabilities { true, false, true, 0 };
        double timeoutSeconds = 120.0;
    };

    OpenAiProvider(Settings settings, std::shared_ptr<HttpClient> client);

    std::string name() const override { return settings.displayName; }
    ProviderCapabilities capabilities() const override { return settings.capabilities; }
    ChatResponse complete(const ChatRequest& request, const CancelToken& cancel, const StreamHandler& onStream) override;

private:
    Settings settings;
    std::shared_ptr<HttpClient> client;
};
}
