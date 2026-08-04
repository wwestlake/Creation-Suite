#include <creation/services/SuiteAiSettings.h>
#include <creation/services/SuiteAiProviderRuntime.h>
#include <creation/services/SuiteVfsServiceClient.h>

namespace
{
juce::var toVar(const creation::services::SuiteAiSettings& settings)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("defaultAccountId", settings.defaultAccountId);

    juce::Array<juce::var> accounts;
    for (const auto& account : settings.accounts)
    {
        auto* accountObject = new juce::DynamicObject();
        accountObject->setProperty("accountId", account.accountId);
        accountObject->setProperty("providerId", account.providerId);
        accountObject->setProperty("accountLabel", account.accountLabel);
        accountObject->setProperty("baseUrl", account.baseUrl);
        accountObject->setProperty("modelName", account.modelName);
        accountObject->setProperty("apiKey", account.apiKey);
        accountObject->setProperty("enabled", account.enabled);
        accounts.add(juce::var(accountObject));
    }

    object->setProperty("accounts", accounts);

    juce::Array<juce::var> appSelections;
    for (const auto& selection : settings.appSelections)
    {
        auto* selectionObject = new juce::DynamicObject();
        selectionObject->setProperty("appDomain", creation::assets::toStorageToken(selection.appDomain));
        selectionObject->setProperty("accountId", selection.accountId);
        selectionObject->setProperty("modelNameOverride", selection.modelNameOverride);
        selectionObject->setProperty("enabled", selection.enabled);
        appSelections.add(juce::var(selectionObject));
    }

    object->setProperty("appSelections", appSelections);
    return juce::var(object);
}

creation::services::SuiteAiSettings fromVar(const juce::var& value)
{
    creation::services::SuiteAiSettings settings;
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return settings;

    settings.defaultAccountId = object->getProperty("defaultAccountId").toString();
    if (const auto* accounts = object->getProperty("accounts").getArray())
    {
        for (const auto& accountValue : *accounts)
        {
            const auto* accountObject = accountValue.getDynamicObject();
            if (accountObject == nullptr)
                continue;

            creation::services::SuiteAiAccountSettings account;
            account.accountId = accountObject->getProperty("accountId").toString();
            account.providerId = accountObject->getProperty("providerId").toString();
            account.accountLabel = accountObject->getProperty("accountLabel").toString();
            account.baseUrl = accountObject->getProperty("baseUrl").toString();
            account.modelName = accountObject->getProperty("modelName").toString();
            account.apiKey = accountObject->getProperty("apiKey").toString();
            account.enabled = static_cast<bool>(accountObject->getProperty("enabled"));
            settings.accounts.add(account);
        }
    }

    if (const auto* appSelections = object->getProperty("appSelections").getArray())
    {
        for (const auto& selectionValue : *appSelections)
        {
            const auto* selectionObject = selectionValue.getDynamicObject();
            if (selectionObject == nullptr)
                continue;

            creation::services::SuiteAiSettings::AppSelection selection;
            selection.appDomain = creation::assets::suiteAppDomainFromStorageToken(
                selectionObject->getProperty("appDomain").toString());
            selection.accountId = selectionObject->getProperty("accountId").toString();
            selection.modelNameOverride = selectionObject->getProperty("modelNameOverride").toString();
            selection.enabled = static_cast<bool>(selectionObject->getProperty("enabled"));
            settings.appSelections.add(selection);
        }
    }

    return settings;
}
}

namespace creation::services
{
juce::Array<SuiteAiProviderPreset> SuiteAiProviderCatalog::createDefaultCatalog()
{
    juce::Array<SuiteAiProviderPreset> providers;
    providers.add({ "openai", "OpenAI", "https://api.openai.com/v1", true, true, true });
    providers.add({ "anthropic", "Anthropic", "https://api.anthropic.com", true, true, false });
    providers.add({ "google", "Google Gemini", "https://generativelanguage.googleapis.com", true, true, false });
    providers.add({ "xai", "xAI", "https://api.x.ai/v1", true, true, true });
    providers.add({ "openrouter", "OpenRouter", "https://openrouter.ai/api/v1", true, true, true });
    providers.add({ "groq", "Groq", "https://api.groq.com/openai/v1", true, true, true });
    providers.add({ "mistral", "Mistral", "https://api.mistral.ai/v1", true, true, false });
    providers.add({ "cohere", "Cohere", "https://api.cohere.com", true, true, false });
    providers.add({ "fireworks", "Fireworks", "https://api.fireworks.ai/inference/v1", true, true, true });
    providers.add({ "deepseek", "DeepSeek", "https://api.deepseek.com/v1", true, true, true });
    providers.add({ "together", "Together", "https://api.together.xyz/v1", true, true, true });
    providers.add({ "perplexity", "Perplexity", "https://api.perplexity.ai", true, true, false });
    providers.add({ "azure-openai", "Azure OpenAI", "", true, true, true });
    providers.add({ "ollama", "Ollama", "http://localhost:11434", false, true, true });
    providers.add({ "lm-studio", "LM Studio", "http://localhost:1234/v1", false, true, true });
    providers.add({ "custom-openai", "Custom OpenAI-Compatible", "", true, true, true });
    return providers;
}

const SuiteAiProviderPreset* SuiteAiProviderCatalog::findById(const juce::Array<SuiteAiProviderPreset>& providers,
                                                              const juce::String& providerId) noexcept
{
    for (const auto& provider : providers)
        if (provider.id == providerId)
            return std::addressof(provider);

    return nullptr;
}

SuiteAiSettings SuiteAiSettingsStore::load(juce::String& errorMessage) const
{
    SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return {};
    }

    juce::MemoryBlock data;
    if (! client.readEntry("ai-settings.json", data))
        return {}; // no entry yet -- same "nothing saved" meaning the old missing-file case had.

    const auto parsed = juce::JSON::parse(juce::String::createStringFromData(data.getData(), static_cast<int>(data.getSize())));
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the suite AI settings entry.";
        return {};
    }

    return fromVar(parsed);
}

bool SuiteAiSettingsStore::save(const SuiteAiSettings& settings, juce::String& errorMessage) const
{
    SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    const auto json = juce::JSON::toString(toVar(settings), true);
    const juce::MemoryBlock data(json.toRawUTF8(), json.getNumBytesAsUTF8());
    if (! client.writeEntry("ai-settings.json", data))
    {
        errorMessage = "Could not save the suite AI settings entry.";
        return false;
    }

    return true;
}

const SuiteAiAccountSettings* SuiteAiSettingsResolver::findAccountById(const SuiteAiSettings& settings,
                                                                       const juce::String& accountId) noexcept
{
    for (const auto& account : settings.accounts)
        if (account.accountId == accountId)
            return std::addressof(account);

    return nullptr;
}

const SuiteAiAccountSettings* SuiteAiSettingsResolver::resolveDefaultAccount(const SuiteAiSettings& settings) noexcept
{
    if (const auto* account = findAccountById(settings, settings.defaultAccountId))
        return account;

    for (const auto& account : settings.accounts)
        if (account.enabled)
            return std::addressof(account);

    return nullptr;
}

const SuiteAiSettings::AppSelection* SuiteAiSettingsResolver::findAppSelection(
    const SuiteAiSettings& settings,
    creation::assets::SuiteAppDomain appDomain) noexcept
{
    for (const auto& selection : settings.appSelections)
        if (selection.appDomain == appDomain)
            return std::addressof(selection);

    return nullptr;
}

const SuiteAiAccountSettings* SuiteAiSettingsResolver::resolveAccountForApp(
    const SuiteAiSettings& settings,
    creation::assets::SuiteAppDomain appDomain) noexcept
{
    if (const auto* selection = findAppSelection(settings, appDomain))
    {
        if (selection->enabled)
            if (const auto* account = findAccountById(settings, selection->accountId))
                if (account->enabled)
                    return account;
    }

    return resolveDefaultAccount(settings);
}

juce::String SuiteAiSettingsResolver::resolveModelNameForApp(const SuiteAiSettings& settings,
                                                             creation::assets::SuiteAppDomain appDomain) noexcept
{
    if (const auto* selection = findAppSelection(settings, appDomain))
        if (selection->enabled && selection->modelNameOverride.isNotEmpty())
            return selection->modelNameOverride;

    if (const auto* account = resolveAccountForApp(settings, appDomain))
        return account->modelName;

    return {};
}

SuiteAiResolvedRuntimeSettings SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
    const SuiteAiSettings& settings,
    creation::assets::SuiteAppDomain appDomain) noexcept
{
    SuiteAiResolvedRuntimeSettings runtimeSettings;
    if (const auto* account = resolveAccountForApp(settings, appDomain))
    {
        runtimeSettings.accountId = account->accountId;
        runtimeSettings.providerId = SuiteAiProviderRuntime::normalizeProviderId(account->providerId);
        runtimeSettings.providerDisplayName = SuiteAiProviderRuntime::resolveProfile(account->providerId).displayName;
        runtimeSettings.baseUrl = account->baseUrl;
        runtimeSettings.modelName = resolveModelNameForApp(settings, appDomain);
        if (runtimeSettings.modelName.isEmpty())
            runtimeSettings.modelName = account->modelName;
        runtimeSettings.apiKey = account->apiKey;
    }

    return runtimeSettings;
}

void SuiteAiSettingsResolver::upsertRuntimeSettingsForApp(SuiteAiSettings& settings,
                                                          creation::assets::SuiteAppDomain appDomain,
                                                          const SuiteAiResolvedRuntimeSettings& runtimeSettings,
                                                          const juce::String& accountLabel,
                                                          bool setAsDefaultIfEmpty)
{
    auto accountId = runtimeSettings.accountId.trim();
    if (accountId.isEmpty())
    {
        if (const auto* existingSelection = findAppSelection(settings, appDomain))
            accountId = existingSelection->accountId.trim();
    }

    if (accountId.isEmpty())
        accountId = "creation-" + creation::assets::toStorageToken(appDomain) + "-primary";

    SuiteAiAccountSettings updatedAccount;
    updatedAccount.accountId = accountId;
    updatedAccount.providerId = SuiteAiProviderRuntime::normalizeProviderId(runtimeSettings.providerId.isNotEmpty()
                                                                                ? runtimeSettings.providerId
                                                                                : runtimeSettings.providerDisplayName);
    updatedAccount.accountLabel = accountLabel;
    updatedAccount.baseUrl = runtimeSettings.baseUrl.trim();
    updatedAccount.modelName = runtimeSettings.modelName.trim();
    updatedAccount.apiKey = runtimeSettings.apiKey;
    updatedAccount.enabled = true;

    auto accountUpdated = false;
    for (auto& account : settings.accounts)
    {
        if (account.accountId == accountId)
        {
            account = updatedAccount;
            accountUpdated = true;
            break;
        }
    }

    if (! accountUpdated)
        settings.accounts.add(updatedAccount);

    auto selectionUpdated = false;
    for (auto& selection : settings.appSelections)
    {
        if (selection.appDomain == appDomain)
        {
            selection.accountId = accountId;
            selection.modelNameOverride.clear();
            selection.enabled = true;
            selectionUpdated = true;
            break;
        }
    }

    if (! selectionUpdated)
        settings.appSelections.add({ appDomain, accountId, {}, true });

    if (setAsDefaultIfEmpty && settings.defaultAccountId.trim().isEmpty())
        settings.defaultAccountId = accountId;
}
}
