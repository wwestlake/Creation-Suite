#pragma once

#include <juce_core/juce_core.h>
#include <creation/assets/ProjectManifest.h>

namespace creation::services
{
struct SuiteAiProviderPreset
{
    juce::String id;
    juce::String displayName;
    juce::String defaultBaseUrl;
    bool requiresApiKey = true;
    bool supportsModelCatalog = true;
    bool supportsOpenAiChatStyle = false;
};

struct SuiteAiAccountSettings
{
    juce::String accountId;
    juce::String providerId;
    juce::String accountLabel;
    juce::String baseUrl;
    juce::String modelName;
    juce::String apiKey;
    bool enabled = true;

    // Populated by "Refresh" in the Add/Edit Account dialog (SuiteSettingsPanel), which calls the
    // provider's real model-list API for this account's key. Cached here, per account, so every
    // app in the suite reads the same list instead of re-querying the provider itself.
    juce::StringArray cachedModelIds;
    juce::String modelsFetchedAt;
};

struct SuiteAiSettings
{
    struct AppSelection
    {
        creation::assets::SuiteAppDomain appDomain = creation::assets::SuiteAppDomain::unknown;
        juce::String accountId;
        juce::String modelNameOverride;
        bool enabled = true;
    };

    juce::Array<SuiteAiAccountSettings> accounts;
    juce::String defaultAccountId;
    juce::Array<AppSelection> appSelections;
};

struct SuiteAiResolvedRuntimeSettings
{
    juce::String accountId;
    juce::String providerId { "openai" };
    juce::String providerDisplayName { "OpenAI" };
    juce::String baseUrl { "https://api.openai.com/v1" };
    juce::String modelName { "gpt-4.1-mini" };
    juce::String apiKey;

    bool isValid() const noexcept { return providerId.isNotEmpty() || providerDisplayName.isNotEmpty(); }
};

class SuiteAiSettingsStore final
{
public:
    SuiteAiSettings load(juce::String& errorMessage) const;
    bool save(const SuiteAiSettings& settings, juce::String& errorMessage) const;

    // Reconnects to every account that has what it needs (a key, or a keyless local provider)
    // and re-caches its model list, then persists the result. Meant to run once per app startup,
    // not polled continuously - the Add/Edit Account dialog's own Connect button remains the
    // manual per-account trigger. An account that fails to connect keeps its existing cache
    // rather than being cleared out by a transient network hiccup.
    SuiteAiSettings refreshAllAccountModelCaches(juce::String& errorMessage) const;
};

class SuiteAiProviderCatalog
{
public:
    static juce::Array<SuiteAiProviderPreset> createDefaultCatalog();
    static const SuiteAiProviderPreset* findById(const juce::Array<SuiteAiProviderPreset>& providers,
                                                 const juce::String& providerId) noexcept;
};

class SuiteAiSettingsResolver
{
public:
    static const SuiteAiAccountSettings* findAccountById(const SuiteAiSettings& settings,
                                                         const juce::String& accountId) noexcept;
    static const SuiteAiAccountSettings* resolveDefaultAccount(const SuiteAiSettings& settings) noexcept;
    static const SuiteAiSettings::AppSelection* findAppSelection(const SuiteAiSettings& settings,
                                                                 creation::assets::SuiteAppDomain appDomain) noexcept;
    static const SuiteAiAccountSettings* resolveAccountForApp(const SuiteAiSettings& settings,
                                                              creation::assets::SuiteAppDomain appDomain) noexcept;
    static juce::String resolveModelNameForApp(const SuiteAiSettings& settings,
                                               creation::assets::SuiteAppDomain appDomain) noexcept;
    static SuiteAiResolvedRuntimeSettings resolveRuntimeSettingsForApp(
        const SuiteAiSettings& settings,
        creation::assets::SuiteAppDomain appDomain) noexcept;
    static void upsertRuntimeSettingsForApp(SuiteAiSettings& settings,
                                            creation::assets::SuiteAppDomain appDomain,
                                            const SuiteAiResolvedRuntimeSettings& runtimeSettings,
                                            const juce::String& accountLabel,
                                            bool setAsDefaultIfEmpty = true);

    // Points an app at an already-configured suite account - never creates or edits an account.
    // Accounts are only ever created/edited at the suite level (SuiteSettingsPanel's Add Account
    // dialog); every app just selects among them via this.
    static void selectAccountForApp(SuiteAiSettings& settings,
                                    creation::assets::SuiteAppDomain appDomain,
                                    const juce::String& accountId,
                                    const juce::String& modelNameOverride = {});
};
}
