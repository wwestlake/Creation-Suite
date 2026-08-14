#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
// Calls a provider's real model-list API (using SuiteAiProviderRuntime's per-provider endpoint
// config) and returns the model IDs actually available to the given key. Used by the "Refresh"
// button in SuiteSettingsPanel's Add/Edit Account dialog - moved here (was CreationStation-local)
// so every app's suite-level account UI can share one implementation instead of each app
// re-implementing its own provider model-list fetch.
class SuiteAiModelCatalogClient final
{
public:
    bool fetchModelIds(const juce::String& baseUrl,
                       const juce::String& providerName,
                       const juce::String& apiKey,
                       juce::StringArray& modelIds,
                       juce::String& errorMessage) const;
};
}
