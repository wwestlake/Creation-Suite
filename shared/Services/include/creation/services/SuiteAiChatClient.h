#pragma once

#include <juce_core/juce_core.h>
#include <creation/services/SuiteAiSettings.h>

#include <vector>

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

    // Tool calling (docs/architecture/Suite-Agent-Runtime-Spec.md, section 12): one model turn that may answer or ask
    // for tools to be run. The caller runs them and sends the results back in the next turn.
    struct ToolCall
    {
        juce::String id;
        juce::String name;
        juce::String arguments; // JSON text, exactly as the model sent it
    };
    struct Message
    {
        juce::String role; // "system", "user", "assistant", "tool"
        juce::String content;
        std::vector<ToolCall> toolCalls; // an assistant turn's requests
        juce::String toolCallId;         // a tool result: which request it answers
    };
    struct ToolSpec
    {
        juce::String name; // letters, digits, _ and - only (the providers' rule)
        juce::String description;
        juce::var parameters; // JSON Schema
    };
    struct TurnResult
    {
        juce::String text;
        std::vector<ToolCall> toolCalls;
        juce::String errorMessage;
        int statusCode = 0;
    };

    // Whether the account's provider can be sent tools. Today: the OpenAI-style providers.
    static bool supportsToolCalling(const SuiteAiResolvedRuntimeSettings& settings);
    bool sendTurn(const SuiteAiResolvedRuntimeSettings& settings,
                  const std::vector<Message>& messages,
                  const std::vector<ToolSpec>& tools,
                  TurnResult& result) const;
};
}
