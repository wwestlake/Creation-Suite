#pragma once

#include <creation/services/SuiteAiSettings.h>

namespace creation::services
{
struct SuiteAiProviderRuntimeProfile
{
    juce::String providerId { "openai" };
    juce::String displayName { "OpenAI" };
    juce::String defaultBaseUrl { "https://api.openai.com/v1" };
    bool requiresApiKey = true;
    bool supportsModelCatalog = true;
    bool supportsOpenAiChatStyle = true;
    juce::String modelCatalogPath { "/models" };
    juce::String modelCatalogArrayProperty { "data" };
    juce::String modelCatalogIdProperty { "id" };
    juce::String chatCompletionsPath { "/chat/completions" };
    juce::String chatResponseTextProperty { "choices[0].message.content" };

    bool isOllamaStyle() const noexcept { return providerId == "ollama"; }
};

class SuiteAiProviderRuntime final
{
public:
    static juce::String normalizeProviderId(const juce::String& providerNameOrId);
    static juce::Array<SuiteAiProviderRuntimeProfile> createChatCapableProfiles();
    static SuiteAiProviderRuntimeProfile resolveProfile(const juce::String& providerNameOrId);
    static juce::String defaultModelName(const SuiteAiProviderRuntimeProfile& profile);
    static juce::String normalizeBaseUrl(const juce::String& baseUrl, const SuiteAiProviderRuntimeProfile& profile);
    static bool shouldReplaceBaseUrlOnProviderSwitch(const juce::String& currentBaseUrl,
                                                     const SuiteAiProviderRuntimeProfile& nextProfile) noexcept;
    static juce::String buildAuthHeaders(const SuiteAiProviderRuntimeProfile& profile, const juce::String& apiKey);
    static bool requiresApiKey(const SuiteAiProviderRuntimeProfile& profile, const juce::String& apiKey) noexcept;
};
}
