#include <creation/services/SuiteAiSettings.h>
#include <creation/services/SuiteAiOrchestration.h>
#include <creation/services/SuiteAiProviderRuntime.h>
#include <creation/services/SuiteLegalSettings.h>
#include <creation/assets/ProjectManifest.h>

#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        creation::services::SuiteAiSettingsStore aiStore;
        creation::services::SuiteLegalSettingsStore legalStore;

        auto aiFile = aiStore.getSettingsFile();
        auto legalFile = legalStore.getSettingsFile();
        aiFile.deleteFile();
        legalFile.deleteFile();

        creation::services::SuiteAiSettings aiSettings;
        aiSettings.defaultAccountId = "primary";
        aiSettings.accounts.add({ "primary", "openai", "Main OpenAI", "https://api.openai.com/v1", "gpt-test", "secret", true });
        aiSettings.accounts.add({ "local", "ollama", "Local Ollama", "http://localhost:11434", "llama3.2:3b", "", true });
        aiSettings.appSelections.add({
            creation::assets::SuiteAppDomain::movie,
            "primary",
            "gpt-movie",
            true
        });

        juce::String errorMessage;
        if (! aiStore.save(aiSettings, errorMessage))
            fail("Failed saving AI settings: " + errorMessage.toStdString());

        auto loadedAi = aiStore.load(errorMessage);
        if (loadedAi.defaultAccountId != "primary" || loadedAi.accounts.size() != 2)
            fail("AI settings round-trip mismatch.");

        const auto* resolvedAccount = creation::services::SuiteAiSettingsResolver::resolveDefaultAccount(loadedAi);
        if (resolvedAccount == nullptr || resolvedAccount->providerId != "openai")
            fail("Default AI account resolution failed.");

        const auto* movieAccount = creation::services::SuiteAiSettingsResolver::resolveAccountForApp(
            loadedAi, creation::assets::SuiteAppDomain::movie);
        if (movieAccount == nullptr || movieAccount->accountId != "primary")
            fail("Per-app AI account resolution failed.");

        const auto movieModel = creation::services::SuiteAiSettingsResolver::resolveModelNameForApp(
            loadedAi, creation::assets::SuiteAppDomain::movie);
        if (movieModel != "gpt-movie")
            fail("Per-app AI model resolution failed.");

        const auto movieRuntime = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
            loadedAi, creation::assets::SuiteAppDomain::movie);
        if (! movieRuntime.isValid()
            || movieRuntime.providerId != "openai"
            || movieRuntime.providerDisplayName != "OpenAI"
            || movieRuntime.modelName != "gpt-movie")
        {
            fail("Per-app runtime AI settings resolution failed.");
        }

        creation::services::SuiteAiResolvedRuntimeSettings liveRuntime;
        liveRuntime.providerId = "ollama";
        liveRuntime.providerDisplayName = "Ollama";
        liveRuntime.baseUrl = "http://localhost:11434";
        liveRuntime.modelName = "llama3.2:3b";
        liveRuntime.apiKey.clear();
        creation::services::SuiteAiSettingsResolver::upsertRuntimeSettingsForApp(
            loadedAi,
            creation::assets::SuiteAppDomain::live,
            liveRuntime,
            "Creation Live");
        const auto resolvedLiveRuntime = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
            loadedAi, creation::assets::SuiteAppDomain::live);
        if (! resolvedLiveRuntime.isValid()
            || resolvedLiveRuntime.providerId != "ollama"
            || resolvedLiveRuntime.modelName != "llama3.2:3b")
        {
            fail("Per-app runtime AI upsert failed.");
        }

        const auto providers = creation::services::SuiteAiProviderCatalog::createDefaultCatalog();
        const auto* provider = creation::services::SuiteAiProviderCatalog::findById(providers, "openai");
        if (provider == nullptr || provider->displayName != "OpenAI")
            fail("Provider catalog lookup failed.");

        const auto runtimeProfile = creation::services::SuiteAiProviderRuntime::resolveProfile("Ollama");
        if (! runtimeProfile.isOllamaStyle()
            || runtimeProfile.modelCatalogPath != "/api/tags"
            || runtimeProfile.chatCompletionsPath != "/api/chat")
        {
            fail("Provider runtime profile resolution failed.");
        }

        creation::services::SuiteAiRequestDescriptor cloudRequest;
        cloudRequest.appDomain = creation::assets::SuiteAppDomain::movie;
        cloudRequest.requiredCapabilities = creation::services::SuiteAiCapabilitySet::fromList({
            creation::services::SuiteAiCapability::textChat,
            creation::services::SuiteAiCapability::visionInput
        });
        auto cloudDecision = creation::services::SuiteAiOrchestrator::planRoutes(aiSettings, cloudRequest);
        if (! cloudDecision.isRoutable()
            || cloudDecision.candidates.getFirst().accountId != "primary")
        {
            fail("Cloud route planning failed.");
        }

        creation::services::SuiteAiRequestDescriptor localRequest;
        localRequest.appDomain = creation::assets::SuiteAppDomain::station;
        localRequest.preferLocalRuntime = true;
        localRequest.requiredCapabilities = creation::services::SuiteAiCapabilitySet::fromList({
            creation::services::SuiteAiCapability::textChat,
            creation::services::SuiteAiCapability::streaming
        });
        auto localDecision = creation::services::SuiteAiOrchestrator::planRoutes(aiSettings, localRequest);
        if (! localDecision.isRoutable()
            || localDecision.candidates.getFirst().accountId != "local")
        {
            fail("Local-preferred route planning failed.");
        }

        juce::Array<creation::services::SuiteAiProviderHealthSnapshot> healthSnapshots;
        healthSnapshots.add({
            "local",
            creation::services::SuiteAiHealthState::coolingDown,
            2,
            1,
            125,
            juce::Time::getCurrentTime() + juce::RelativeTime::seconds(30),
            "rate limited"
        });
        auto cooledDecision = creation::services::SuiteAiOrchestrator::planRoutes(aiSettings, localRequest, healthSnapshots);
        if (! cooledDecision.isRoutable()
            || cooledDecision.candidates.getFirst().accountId != "primary")
        {
            fail("Cooldown fallback route planning failed.");
        }

        creation::services::SuiteLegalSettings legalSettings;
        legalSettings.eulaAccepted = true;
        legalSettings.acceptedEulaVersion = "2026-07-29";
        legalSettings.acceptedAt = juce::Time::getCurrentTime();

        if (! legalStore.save(legalSettings, errorMessage))
            fail("Failed saving legal settings: " + errorMessage.toStdString());

        auto loadedLegal = legalStore.load(errorMessage);
        if (! loadedLegal.eulaAccepted || loadedLegal.acceptedEulaVersion != "2026-07-29")
            fail("Legal settings round-trip mismatch.");

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ServicesSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
