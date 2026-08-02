#pragma once

#include <juce_core/juce_core.h>
#include <creation/services/SuiteAiSettings.h>

namespace creation::services
{
// Suite-wide AI completion transport -- the one piece of the BYOK
// subsystem that actually sends a prompt over the network and gets a
// response back. Promoted from Creation Station's original app-local
// OpenAiChatClient (which was already fully provider-agnostic,
// delegating every provider-specific detail to SuiteAiProviderRuntime)
// so every app gets a real, working completion call, not just settings
// resolution and a chat UI with nothing behind it.
class SuiteAiChatClient final
{
public:
    struct ChatResult
    {
        juce::String text;
        juce::String rawResponse;
        juce::String errorMessage;
        int statusCode = 0;
    };

    bool sendChatCompletion(const SuiteAiResolvedRuntimeSettings& settings,
                            const juce::String& systemPrompt,
                            const juce::String& userPrompt,
                            ChatResult& result) const;
};
}
