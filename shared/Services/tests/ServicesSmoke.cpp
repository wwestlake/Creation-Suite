#include <creation/services/SuiteAiSettings.h>
#include <creation/services/SuiteAiOrchestration.h>
#include <creation/services/SuiteAiProviderRuntime.h>
#include <creation/services/SuiteAiService.h>
#include <creation/services/SuiteLegalSettings.h>
#include <creation/services/SuiteLogging.h>
#include <creation/services/SuiteProcessRegistry.h>
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

        creation::services::SuiteAiRequestDescriptor noCrossFallbackRequest = localRequest;
        noCrossFallbackRequest.preferredProviderId = "ollama";
        noCrossFallbackRequest.fallbackPolicy.allowCrossProviderFallback = false;
        auto noCrossFallbackDecision = creation::services::SuiteAiOrchestrator::planRoutes(aiSettings,
                                                                                           noCrossFallbackRequest,
                                                                                           healthSnapshots);
        if (noCrossFallbackDecision.isRoutable())
            fail("Cross-provider fallback policy should have blocked fallback.");

        creation::services::SuiteAiRequestDescriptor budgetRequest = cloudRequest;
        budgetRequest.budgetPolicy.enforceBudget = true;
        budgetRequest.budgetPolicy.maxEstimatedCostUsd = 0.04;
        auto budgetDecision = creation::services::SuiteAiOrchestrator::planRoutes(aiSettings, budgetRequest);
        if (budgetDecision.isRoutable())
            fail("Budget enforcement should have blocked the premium cloud route.");

        creation::services::SuiteAiSettings missingKeySettings = aiSettings;
        missingKeySettings.accounts.clear();
        missingKeySettings.defaultAccountId = "broken";
        missingKeySettings.accounts.add({ "broken", "openai", "Broken OpenAI", "https://api.openai.com/v1", "gpt-test", "", true });
        auto missingKeyDecision = creation::services::SuiteAiOrchestrator::planRoutes(missingKeySettings, cloudRequest);
        if (missingKeyDecision.isRoutable())
            fail("Missing API key should block a key-required provider.");

        creation::services::SuiteAiService aiService;
        auto planningResult = aiService.planRequest(cloudRequest, errorMessage);
        if (errorMessage.isNotEmpty()
            || ! planningResult.routingDecision.isRoutable()
            || planningResult.routingDecision.candidates.getFirst().accountId != "primary")
        {
            fail("Suite AI service route planning failed.");
        }

        creation::services::SuiteAiDiagnosticsEvent event;
        event.eventType = "route-planned";
        event.providerId = "openai";
        event.accountId = "primary";
        event.modelName = "gpt-movie";
        event.message = "Planned route for movie helper.";
        event.retryCount = 0;
        event.fallbackUsed = false;
        event.queueDurationMs = 3;
        event.latencyMs = 42;
        if (! aiService.recordDiagnosticsEvent(event, errorMessage, 10))
            fail("Suite AI diagnostics append failed: " + errorMessage.toStdString());

        auto diagnosticsLog = aiService.loadDiagnosticsLog(errorMessage);
        if (errorMessage.isNotEmpty()
            || diagnosticsLog.events.isEmpty()
            || diagnosticsLog.events.getLast().eventType != "route-planned")
        {
            fail("Suite AI diagnostics load failed.");
        }

        if (! aiService.recordRouteOutcome("primary", false, true, 150, "rate limited", errorMessage))
            fail("Suite AI route outcome recording failed: " + errorMessage.toStdString());

        auto healthAfterRateLimit = aiService.loadHealthSnapshots(errorMessage);
        const auto rateLimitedSnapshot = std::find_if(healthAfterRateLimit.begin(), healthAfterRateLimit.end(),
                                                      [](const auto& snapshot) { return snapshot.accountId == "primary"; });
        if (errorMessage.isNotEmpty()
            || rateLimitedSnapshot == healthAfterRateLimit.end()
            || rateLimitedSnapshot->healthState != creation::services::SuiteAiHealthState::coolingDown)
        {
            fail("Suite AI health snapshot recording failed.");
        }

        auto activityLog = aiService.loadActivityLog(errorMessage, 20);
        if (errorMessage.isNotEmpty()
            || activityLog.isEmpty()
            || activityLog.getLast().subsystem != "suite-ai")
        {
            fail("Suite activity log load failed.");
        }

        creation::services::SuiteLogEntry manualLogEntry = creation::services::SuiteLogger::makeEntry(
            "suite-core",
            "bootstrap",
            "Centralized suite logging is online.",
            creation::services::SuiteLogLevel::info,
            creation::assets::SuiteAppDomain::modeler,
            "manual smoke test");
        if (! creation::services::SuiteLogger::log(manualLogEntry, errorMessage, 20))
            fail("Suite manual log append failed: " + errorMessage.toStdString());

        creation::services::SuiteLogStore logStore;
        auto recentEntries = logStore.loadRecent(5, errorMessage);
        if (errorMessage.isNotEmpty()
            || recentEntries.isEmpty()
            || recentEntries.getLast().subsystem != "suite-core")
        {
            fail("Suite centralized logging verification failed.");
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

        // --- IPC-1: SuiteProcessRegistry -----------------------------
        {
            creation::services::SuiteProcessRegistration freshRegistration;
            freshRegistration.RegisterSelf("TestAppFresh", 9000, "TestAppFresh-pipe");

            creation::services::SuiteProcessRegistration staleRegistration;
            staleRegistration.RegisterSelf("TestAppStale", 9001, "TestAppStale-pipe");

            // Backdate the stale registration's own file directly on disk
            // (bypassing the real 5s heartbeat interval, which a fast
            // smoke test shouldn't have to wait out) so
            // EnumerateLiveProcesses has something genuinely old to
            // filter, deterministically, without a real sleep.
            auto findFileFor = [](const juce::String& appId) -> juce::File {
                juce::Array<juce::File> files;
                creation::services::SuiteProcessRegistry::RegistryDirectory().findChildFiles(
                    files, juce::File::findFiles, false, appId + "-*.json");
                return files.isEmpty() ? juce::File() : files.getFirst();
            };

            auto staleFile = findFileFor("TestAppStale");
            if (! staleFile.existsAsFile())
                fail("SuiteProcessRegistry: stale test registration file was not written.");

            auto staleJson = juce::JSON::parse(staleFile);
            if (auto* object = staleJson.getDynamicObject())
            {
                const auto ancientMs = (juce::Time::getCurrentTime() - juce::RelativeTime::seconds(1000)).toMilliseconds();
                object->setProperty("lastHeartbeatMs", ancientMs);
                staleFile.replaceWithText(juce::JSON::toString(staleJson, true));
            }
            else
            {
                fail("SuiteProcessRegistry: could not parse stale test registration file to backdate it.");
            }

            const auto live = creation::services::SuiteProcessRegistry::EnumerateLiveProcesses(15.0);

            const auto findRecord = [&live](const juce::String& appId) {
                return std::find_if(live.begin(), live.end(),
                                    [&appId](const auto& record) { return record.appId == appId; });
            };

            const auto freshRecord = findRecord("TestAppFresh");
            if (freshRecord == live.end() || freshRecord->oscPort != 9000 || freshRecord->pipeName != "TestAppFresh-pipe")
                fail("SuiteProcessRegistry: fresh registration was not enumerated correctly.");

            if (findRecord("TestAppStale") != live.end())
                fail("SuiteProcessRegistry: stale registration should have been excluded.");

            if (staleFile.existsAsFile())
                fail("SuiteProcessRegistry: stale registration's file should have been deleted during enumeration.");
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ServicesSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
