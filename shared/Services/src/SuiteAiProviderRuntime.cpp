#include <creation/services/SuiteAiProviderRuntime.h>

namespace creation::services
{
juce::String SuiteAiProviderRuntime::normalizeProviderId(const juce::String& providerNameOrId)
{
    auto normalized = providerNameOrId.trim().toLowerCase();
    if (normalized.contains("ollama"))
        return "ollama";

    if (normalized.isEmpty())
        return "openai";

    const auto providers = SuiteAiProviderCatalog::createDefaultCatalog();
    for (const auto& provider : providers)
        if (provider.id.equalsIgnoreCase(normalized) || provider.displayName.equalsIgnoreCase(providerNameOrId))
            return provider.id;

    return normalized.replaceCharacter(' ', '-');
}

juce::Array<SuiteAiProviderRuntimeProfile> SuiteAiProviderRuntime::createChatCapableProfiles()
{
    juce::Array<SuiteAiProviderRuntimeProfile> profiles;
    const auto providers = SuiteAiProviderCatalog::createDefaultCatalog();
    for (const auto& provider : providers)
        if (provider.supportsOpenAiChatStyle || provider.id == "ollama")
            profiles.add(resolveProfile(provider.id));
    return profiles;
}

SuiteAiProviderRuntimeProfile SuiteAiProviderRuntime::resolveProfile(const juce::String& providerNameOrId)
{
    SuiteAiProviderRuntimeProfile profile;
    const auto providerId = normalizeProviderId(providerNameOrId);
    const auto providers = SuiteAiProviderCatalog::createDefaultCatalog();
    if (const auto* provider = SuiteAiProviderCatalog::findById(providers, providerId))
    {
        profile.providerId = provider->id;
        profile.displayName = provider->displayName;
        profile.defaultBaseUrl = provider->defaultBaseUrl.isNotEmpty() ? provider->defaultBaseUrl : profile.defaultBaseUrl;
        profile.requiresApiKey = provider->requiresApiKey;
        profile.supportsModelCatalog = provider->supportsModelCatalog;
        profile.supportsOpenAiChatStyle = provider->supportsOpenAiChatStyle;
    }

    if (profile.providerId == "ollama")
    {
        profile.modelCatalogPath = "/api/tags";
        profile.modelCatalogArrayProperty = "models";
        profile.modelCatalogIdProperty = "name";
        profile.chatCompletionsPath = "/api/chat";
        profile.chatResponseTextProperty = "message.content";
    }

    return profile;
}

juce::String SuiteAiProviderRuntime::defaultModelName(const SuiteAiProviderRuntimeProfile& profile)
{
    if (profile.providerId == "ollama")
        return "llama3.2:3b";

    return "gpt-4.1-mini";
}

juce::String SuiteAiProviderRuntime::normalizeBaseUrl(const juce::String& baseUrl,
                                                      const SuiteAiProviderRuntimeProfile& profile)
{
    auto trimmed = baseUrl.trim();
    if (trimmed.isEmpty())
        trimmed = profile.defaultBaseUrl;

    return trimmed.endsWithChar('/') ? trimmed.dropLastCharacters(1) : trimmed;
}

bool SuiteAiProviderRuntime::shouldReplaceBaseUrlOnProviderSwitch(const juce::String& currentBaseUrl,
                                                                  const SuiteAiProviderRuntimeProfile& nextProfile) noexcept
{
    const auto current = currentBaseUrl.trim().toLowerCase();
    if (current.isEmpty())
        return true;

    if (nextProfile.isOllamaStyle())
        return current.contains("api.openai.com");

    return current.contains("localhost:11434") || current.contains("127.0.0.1:11434");
}

juce::String SuiteAiProviderRuntime::buildAuthHeaders(const SuiteAiProviderRuntimeProfile& profile,
                                                      const juce::String& apiKey)
{
    juce::String headers;
    if (profile.requiresApiKey && apiKey.trim().isNotEmpty())
        headers << "Authorization: Bearer " << apiKey << "\r\n";

    headers << "Content-Type: application/json\r\n";
    headers << "Accept: application/json\r\n";
    return headers;
}

bool SuiteAiProviderRuntime::requiresApiKey(const SuiteAiProviderRuntimeProfile& profile,
                                            const juce::String& apiKey) noexcept
{
    if (! profile.requiresApiKey)
        return false;

    return apiKey.trim().isEmpty();
}
}
