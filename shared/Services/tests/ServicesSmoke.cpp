#include <creation/services/SuiteAiSettings.h>
#include <creation/services/SuiteAiOrchestration.h>
#include <creation/services/SuiteAiProviderRuntime.h>
#include <creation/services/SuiteAiService.h>
#include <creation/services/SuiteLegalSettings.h>
#include <creation/services/SuiteLogging.h>
#include <creation/services/SuiteProcessRegistry.h>
#include <creation/services/SuiteContextEngine.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <creation/assets/ProjectManifest.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/suite/SuiteStoragePaths.h>

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
        creation::services::SuiteAiHealthSnapshotStore healthStore;
        creation::services::SuiteAiDiagnosticsStore diagnosticsStore;
        creation::services::SuiteLogStore logStoreForCleanup;

        // AI settings now live inside the suite root project via
        // SuiteVfsServiceClient (requires services/VfsService to be built
        // and reachable, per docs/architecture/Suite-Shared-Project-Model.md)
        // rather than a loose file -- save() below overwrites whatever was
        // there, so no separate pre-test delete step is needed for it.
        auto legalFile = legalStore.getSettingsFile();
        legalFile.deleteFile();
        // These three persist across runs and are never reset by the settings-file
        // deletes above -- a prior direct run of this executable (e.g. manual
        // debugging) otherwise leaves rate-limit/health state that silently breaks
        // THIS run's "fresh state" assumptions below. Confirmed reproducible.
        healthStore.getSettingsFile().deleteFile();
        diagnosticsStore.getSettingsFile().deleteFile();
        logStoreForCleanup.getSettingsFile().deleteFile();

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

        // --- CTX-1: SuiteContextEngine -------------------------------
        {
            class FixedProvider final : public creation::services::SuiteContextProvider
            {
            public:
                juce::Array<creation::services::SuiteContextDocument> CollectDocuments() override
                {
                    juce::Array<creation::services::SuiteContextDocument> docs;
                    docs.add({ "light-1", "Point Light Setup Notes", "lighting",
                              "Point lights use intensity and falloff radius to control brightness in the scene.",
                              "TestApp", {}, {}, juce::Time::getCurrentTime() });
                    docs.add({ "net-1", "OSC Transport Notes", "networking",
                              "Configure the OSC transport port and pipe name for cross-process trigger delivery.",
                              "TestApp", {}, {}, juce::Time::getCurrentTime() });
                    return docs;
                }
            };

            FixedProvider provider;
            creation::services::SuiteContextEngine engine;
            engine.RegisterProvider(&provider);

            auto submitAndWait = [&engine](const juce::String& prompt) {
                creation::services::SuiteContextRetrievalRequest request;
                request.prompt = prompt;
                request.appDomain = "TestApp";
                engine.SubmitRequest(request);

                creation::services::SuiteContextPacket packet;
                for (int attempt = 0; attempt < 200; ++attempt)
                {
                    packet = engine.GetLastPacket();
                    if (packet.request.prompt == prompt)
                        return packet;
                    juce::Thread::sleep(10);
                }
                return packet;
            };

            // Deliberately reuse exact vocabulary across the anchor and the
            // on-topic follow-up (token-overlap is exact-match, so "light"
            // vs "lights" would count as no overlap at all) and keep the
            // pivot's vocabulary completely disjoint from both, so the drift/
            // velocity comparison below isolates topic change, not wording.
            const auto onTopicA = submitAndWait("point light intensity setup in the scene");
            if (onTopicA.snippets.isEmpty() || onTopicA.snippets.getFirst().documentId != "light-1")
                fail("SuiteContextEngine: first lighting prompt did not retrieve the lighting document.");

            const auto onTopicB = submitAndWait("point light intensity falloff radius in the scene");

            const auto pivot = submitAndWait("osc transport network port and pipe name configuration");
            if (pivot.snippets.isEmpty() || pivot.snippets.getFirst().documentId != "net-1")
                fail("SuiteContextEngine: pivot prompt did not retrieve the networking document.");

            if (! (pivot.dynamics.referenceDrift > onTopicB.dynamics.referenceDrift))
                fail("SuiteContextEngine: reference drift should be higher on a hard topic pivot than a same-topic follow-up.");

            if (! (pivot.dynamics.velocity > onTopicB.dynamics.velocity))
                fail("SuiteContextEngine: velocity should be higher on a hard topic pivot than a same-topic follow-up.");

            if (pivot.dynamics.torsionalResistance <= 0)
                fail("SuiteContextEngine: torsional resistance should accumulate once recovery is suggested on the pivot.");

            const auto recoveryTurn = submitAndWait("point light intensity settings again");
            if (! (recoveryTurn.dynamics.referenceDrift < pivot.dynamics.referenceDrift))
                fail("SuiteContextEngine: returning to the anchor topic should reduce reference drift versus the pivot.");

            // --- CTX-2: user-supplied process instructions -----------
            auto findScore = [](const creation::services::SuiteContextPacket& packet, const juce::String& id) {
                for (const auto& snippet : packet.snippets)
                    if (snippet.documentId == id)
                        return snippet.relevanceScore;
                return -1.0f;
            };

            auto submitWithInstruction = [&engine](const juce::String& prompt,
                                                    const creation::services::SuiteContextProcessInstruction& instruction) {
                creation::services::SuiteContextRetrievalRequest request;
                request.prompt = prompt;
                request.appDomain = "TestApp";
                request.processInstruction = instruction;
                engine.SubmitRequest(request);

                creation::services::SuiteContextPacket packet;
                for (int attempt = 0; attempt < 200; ++attempt)
                {
                    packet = engine.GetLastPacket();
                    if (packet.request.prompt == prompt && packet.request.processInstruction.boostCategories == instruction.boostCategories
                        && packet.request.processInstruction.excludeCategories == instruction.excludeCategories)
                        return packet;
                    juce::Thread::sleep(10);
                }
                return packet;
            };

            const auto ambiguousPrompt = juce::String("show me the setup notes");
            const auto baseline = submitAndWait(ambiguousPrompt);
            const auto baselineNetScore = findScore(baseline, "net-1");

            creation::services::SuiteContextProcessInstruction boostNetworking;
            boostNetworking.boostCategories = { "networking" };
            const auto boosted = submitWithInstruction(ambiguousPrompt, boostNetworking);
            const auto boostedNetScore = findScore(boosted, "net-1");
            if (boostedNetScore < 0.0f || ! (boostedNetScore > baselineNetScore))
                fail("SuiteContextEngine: a boostCategories instruction should raise the matching document's score.");

            creation::services::SuiteContextProcessInstruction excludeNetworking;
            excludeNetworking.excludeCategories = { "networking" };
            const auto excluded = submitWithInstruction(ambiguousPrompt, excludeNetworking);
            if (findScore(excluded, "net-1") >= 0.0f)
                fail("SuiteContextEngine: an excludeCategories instruction should remove the matching document entirely.");

            engine.UnregisterProvider(&provider);
        }

        // --- Project storage: real folders under the configured VFS root, all through the
        // running service (docs/architecture/Suite-Shared-Project-Model.md, VfsProjectStore
        // in services/VfsService). Proves both the new /project/* endpoints and this session's
        // getProjectContainerDirectory path-bug fix (folder must land under the CONFIGURED
        // suiteVfsRoot, not AppData).
        {
            creation::services::SuiteVfsServiceClient client;
            if (! client.discover())
                fail("VfsProjectStore smoke: could not reach the suite VFS service.");

            juce::String suiteSettingsError;
            const auto suiteSettings = creation::suite::SuiteSettingsStore().load(suiteSettingsError);

            juce::String createError, projectId;
            creation::assets::ProjectManifest manifest;
            if (! client.createProject(creation::assets::SuiteAppDomain::station, "Services Smoke Test Project",
                                       "0.0.0-smoke", "0.0.0-smoke", projectId, manifest, createError))
                fail("VfsProjectStore smoke: createProject failed: " + createError.toStdString());

            if (projectId.isEmpty() || manifest.projectId != projectId)
                fail("VfsProjectStore smoke: createProject returned a mismatched projectId.");

            const auto expectedFolder = creation::suite::getProjectContainerDirectory(suiteSettings)
                                            .getChildFile("Creation Station")
                                            .getChildFile(projectId);
            if (! expectedFolder.isDirectory())
                fail("VfsProjectStore smoke: project folder does not exist at the configured VFS root: "
                     + expectedFolder.getFullPathName().toStdString());
            if (expectedFolder.getFullPathName().contains("AppData") && suiteSettings.suiteVfsRoot.isNotEmpty()
                && ! suiteSettings.suiteVfsRoot.contains("AppData"))
                fail("VfsProjectStore smoke: project folder landed in AppData despite a real configured VFS root -- "
                     "the getProjectContainerDirectory bug fix did not take effect.");

            const juce::String entryText = "hello from ServicesSmoke";
            const juce::MemoryBlock entryData(entryText.toRawUTF8(), entryText.getNumBytesAsUTF8());
            if (! client.writeProjectEntry(projectId, "Assets/Source/note.txt", entryData))
                fail("VfsProjectStore smoke: writeProjectEntry failed.");

            juce::MemoryBlock readBack;
            if (! client.readProjectEntry(projectId, "Assets/Source/note.txt", readBack)
                || readBack.toString() != entryText)
                fail("VfsProjectStore smoke: readProjectEntry round-trip mismatch.");

            juce::StringArray entries;
            if (! client.listProjectEntries(projectId, entries) || ! entries.contains("Assets/Source/note.txt"))
                fail("VfsProjectStore smoke: listProjectEntries did not include the written entry.");

            manifest.tags.add("smoke-tested");
            if (! client.writeManifest(projectId, manifest))
                fail("VfsProjectStore smoke: writeManifest failed.");

            creation::assets::ProjectManifest reloadedManifest;
            if (! client.readManifest(projectId, reloadedManifest) || ! reloadedManifest.tags.contains("smoke-tested"))
                fail("VfsProjectStore smoke: readManifest did not reflect the written manifest.");

            juce::Array<creation::services::SuiteVfsServiceClient::ProjectSummary> projects;
            if (! client.listProjects(creation::assets::SuiteAppDomain::station, projects))
                fail("VfsProjectStore smoke: listProjects failed.");
            bool foundInListing = false;
            for (const auto& summary : projects)
                if (summary.projectId == projectId)
                    foundInListing = true;
            if (! foundInListing)
                fail("VfsProjectStore smoke: listProjects did not include the created project.");

            juce::String cloneError, clonedProjectId;
            if (! client.cloneProject(projectId, "Services Smoke Test Project (Clone)", clonedProjectId, cloneError))
                fail("VfsProjectStore smoke: cloneProject failed: " + cloneError.toStdString());
            if (clonedProjectId.isEmpty() || clonedProjectId == projectId)
                fail("VfsProjectStore smoke: cloneProject did not return a distinct new id.");

            juce::MemoryBlock clonedEntry;
            if (! client.readProjectEntry(clonedProjectId, "Assets/Source/note.txt", clonedEntry)
                || clonedEntry.toString() != entryText)
                fail("VfsProjectStore smoke: cloned project's entry did not match the source.");

            if (! client.removeProjectEntry(projectId, "Assets/Source/note.txt"))
                fail("VfsProjectStore smoke: removeProjectEntry failed.");
            juce::MemoryBlock afterRemove;
            if (client.readProjectEntry(projectId, "Assets/Source/note.txt", afterRemove))
                fail("VfsProjectStore smoke: entry still readable after removeProjectEntry.");

            // Regression guard for the existing /suite/entry* contract, reimplemented against
            // VfsProjectStore this pass -- must still round-trip exactly as before.
            const juce::String suiteEntryText = "suite entry regression check";
            const juce::MemoryBlock suiteEntryData(suiteEntryText.toRawUTF8(), suiteEntryText.getNumBytesAsUTF8());
            if (! client.writeEntry("services-smoke-regression.txt", suiteEntryData))
                fail("VfsProjectStore smoke: /suite/entry regression write failed.");
            juce::MemoryBlock suiteReadBack;
            if (! client.readEntry("services-smoke-regression.txt", suiteReadBack)
                || suiteReadBack.toString() != suiteEntryText)
                fail("VfsProjectStore smoke: /suite/entry regression read-back mismatch.");
            client.removeEntry("services-smoke-regression.txt");

            // Clean up only the two project folders this test created -- never touch the rest
            // of "Project Containers" (real projects, or other tests' data, may live alongside).
            expectedFolder.deleteRecursively();
            const auto clonedFolder = creation::suite::getProjectContainerDirectory(suiteSettings)
                                          .getChildFile("Creation Station")
                                          .getChildFile(clonedProjectId);
            clonedFolder.deleteRecursively();
        }

        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "ServicesSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
