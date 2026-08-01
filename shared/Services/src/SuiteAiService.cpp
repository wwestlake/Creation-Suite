#include <creation/services/SuiteAiService.h>
#include <creation/services/SuiteAiSettings.h>
#include <creation/suite/SuiteSettings.h>

namespace
{
juce::File getSuiteConfigDirectory()
{
    return creation::suite::SuiteSettingsStore().getSuiteConfigDirectory();
}

juce::var toVar(const creation::services::SuiteAiProviderHealthSnapshot& snapshot)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("accountId", snapshot.accountId);
    object->setProperty("healthState", static_cast<int>(snapshot.healthState));
    object->setProperty("consecutiveFailures", snapshot.consecutiveFailures);
    object->setProperty("observedRateLimitCount", snapshot.observedRateLimitCount);
    object->setProperty("lastLatencyMs", snapshot.lastLatencyMs);
    object->setProperty("cooldownUntilMs", snapshot.cooldownUntil.toMilliseconds());
    object->setProperty("lastError", snapshot.lastError);
    return juce::var(object);
}

creation::services::SuiteAiProviderHealthSnapshot snapshotFromVar(const juce::var& value)
{
    creation::services::SuiteAiProviderHealthSnapshot snapshot;
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return snapshot;

    snapshot.accountId = object->getProperty("accountId").toString();
    snapshot.healthState = static_cast<creation::services::SuiteAiHealthState>(static_cast<int>(object->getProperty("healthState")));
    snapshot.consecutiveFailures = static_cast<int>(object->getProperty("consecutiveFailures"));
    snapshot.observedRateLimitCount = static_cast<int>(object->getProperty("observedRateLimitCount"));
    snapshot.lastLatencyMs = static_cast<juce::int64>(object->getProperty("lastLatencyMs"));
    snapshot.cooldownUntil = juce::Time(static_cast<juce::int64>(object->getProperty("cooldownUntilMs")));
    snapshot.lastError = object->getProperty("lastError").toString();
    return snapshot;
}

juce::var toVar(const creation::services::SuiteAiDiagnosticsEvent& event)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("eventType", event.eventType);
    object->setProperty("providerId", event.providerId);
    object->setProperty("accountId", event.accountId);
    object->setProperty("modelName", event.modelName);
    object->setProperty("message", event.message);
    object->setProperty("retryCount", event.retryCount);
    object->setProperty("fallbackUsed", event.fallbackUsed);
    object->setProperty("queueDurationMs", event.queueDurationMs);
    object->setProperty("latencyMs", event.latencyMs);
    return juce::var(object);
}

creation::services::SuiteAiDiagnosticsEvent diagnosticsEventFromVar(const juce::var& value)
{
    creation::services::SuiteAiDiagnosticsEvent event;
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return event;

    event.eventType = object->getProperty("eventType").toString();
    event.providerId = object->getProperty("providerId").toString();
    event.accountId = object->getProperty("accountId").toString();
    event.modelName = object->getProperty("modelName").toString();
    event.message = object->getProperty("message").toString();
    event.retryCount = static_cast<int>(object->getProperty("retryCount"));
    event.fallbackUsed = static_cast<bool>(object->getProperty("fallbackUsed"));
    event.queueDurationMs = static_cast<juce::int64>(object->getProperty("queueDurationMs"));
    event.latencyMs = static_cast<juce::int64>(object->getProperty("latencyMs"));
    return event;
}
}

namespace creation::services
{
juce::Array<SuiteAiProviderHealthSnapshot> SuiteAiHealthSnapshotStore::load(juce::String& errorMessage) const
{
    juce::Array<SuiteAiProviderHealthSnapshot> snapshots;
    auto settingsFile = getSettingsFile();
    if (! settingsFile.existsAsFile())
        return snapshots;

    const auto parsed = juce::JSON::parse(settingsFile);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the suite AI health snapshot file.";
        return {};
    }

    if (const auto* object = parsed.getDynamicObject())
        if (const auto* array = object->getProperty("snapshots").getArray())
            for (const auto& value : *array)
                snapshots.add(snapshotFromVar(value));

    return snapshots;
}

bool SuiteAiHealthSnapshotStore::save(const juce::Array<SuiteAiProviderHealthSnapshot>& snapshots,
                                      juce::String& errorMessage) const
{
    auto configDirectory = getSuiteConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the suite configuration folder for AI health snapshots.";
        return false;
    }

    juce::Array<juce::var> values;
    for (const auto& snapshot : snapshots)
        values.add(toVar(snapshot));

    auto* object = new juce::DynamicObject();
    object->setProperty("snapshots", values);
    if (! getSettingsFile().replaceWithText(juce::JSON::toString(juce::var(object), true)))
    {
        errorMessage = "Could not save the suite AI health snapshot file.";
        return false;
    }

    return true;
}

juce::File SuiteAiHealthSnapshotStore::getSettingsFile() const
{
    return getSuiteConfigDirectory().getChildFile("suite-ai-health.json");
}

SuiteAiDiagnosticsLog SuiteAiDiagnosticsStore::load(juce::String& errorMessage) const
{
    SuiteAiDiagnosticsLog log;
    auto settingsFile = getSettingsFile();
    if (! settingsFile.existsAsFile())
        return log;

    const auto parsed = juce::JSON::parse(settingsFile);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the suite AI diagnostics file.";
        return {};
    }

    if (const auto* object = parsed.getDynamicObject())
        if (const auto* array = object->getProperty("events").getArray())
            for (const auto& value : *array)
                log.events.add(diagnosticsEventFromVar(value));

    return log;
}

bool SuiteAiDiagnosticsStore::save(const SuiteAiDiagnosticsLog& log,
                                   juce::String& errorMessage) const
{
    auto configDirectory = getSuiteConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the suite configuration folder for AI diagnostics.";
        return false;
    }

    juce::Array<juce::var> values;
    for (const auto& event : log.events)
        values.add(toVar(event));

    auto* object = new juce::DynamicObject();
    object->setProperty("events", values);
    if (! getSettingsFile().replaceWithText(juce::JSON::toString(juce::var(object), true)))
    {
        errorMessage = "Could not save the suite AI diagnostics file.";
        return false;
    }

    return true;
}

bool SuiteAiDiagnosticsStore::append(const SuiteAiDiagnosticsEvent& event,
                                     int maxEventsToKeep,
                                     juce::String& errorMessage) const
{
    auto log = load(errorMessage);
    if (errorMessage.isNotEmpty())
        return false;

    log.events.add(event);
    while (maxEventsToKeep > 0 && log.events.size() > maxEventsToKeep)
        log.events.remove(0);

    return save(log, errorMessage);
}

juce::File SuiteAiDiagnosticsStore::getSettingsFile() const
{
    return getSuiteConfigDirectory().getChildFile("suite-ai-diagnostics.json");
}

SuiteAiService::RoutePlanningResult SuiteAiService::planRequest(const SuiteAiRequestDescriptor& request,
                                                                juce::String& errorMessage) const
{
    RoutePlanningResult result;

    SuiteAiSettingsStore settingsStore;
    result.settings = settingsStore.load(errorMessage);
    if (errorMessage.isNotEmpty())
        return result;

    SuiteAiHealthSnapshotStore healthStore;
    result.healthSnapshots = healthStore.load(errorMessage);
    if (errorMessage.isNotEmpty())
        return result;

    result.routingDecision = SuiteAiOrchestrator::planRoutes(result.settings, request, result.healthSnapshots);

    juce::String logError;
    auto logEntry = SuiteLogger::makeEntry("suite-ai",
                                           "route-plan",
                                           result.routingDecision.isRoutable()
                                               ? "Planned suite AI route candidates."
                                               : "Suite AI route planning produced no routable candidates.",
                                           result.routingDecision.isRoutable() ? SuiteLogLevel::info : SuiteLogLevel::warning,
                                           request.appDomain,
                                           result.routingDecision.summary());
    SuiteLogger::log(logEntry, logError);
    return result;
}

bool SuiteAiService::recordDiagnosticsEvent(const SuiteAiDiagnosticsEvent& event,
                                            juce::String& errorMessage,
                                            int maxEventsToKeep) const
{
    if (! SuiteAiDiagnosticsStore().append(event, maxEventsToKeep, errorMessage))
        return false;

    juce::String logError;
    auto logEntry = SuiteLogger::makeEntry("suite-ai",
                                           "diagnostics",
                                           "Recorded suite AI diagnostics event.",
                                           SuiteLogLevel::info,
                                           creation::assets::SuiteAppDomain::unknown,
                                           event.eventType + ": " + event.message,
                                           event.accountId);
    SuiteLogger::log(logEntry, logError);
    return true;
}

bool SuiteAiService::recordRouteOutcome(const juce::String& accountId,
                                        bool success,
                                        bool rateLimited,
                                        juce::int64 latencyMs,
                                        const juce::String& errorText,
                                        juce::String& errorMessage) const
{
    SuiteAiHealthSnapshotStore store;
    auto snapshots = store.load(errorMessage);
    if (errorMessage.isNotEmpty())
        return false;

    SuiteAiProviderHealthSnapshot snapshot;
    snapshot.accountId = accountId;
    snapshot.lastLatencyMs = latencyMs;
    snapshot.lastError = errorText;

    if (success)
    {
        snapshot.healthState = SuiteAiHealthState::healthy;
        snapshot.consecutiveFailures = 0;
        snapshot.cooldownUntil = {};
    }
    else if (rateLimited)
    {
        snapshot.healthState = SuiteAiHealthState::coolingDown;
        snapshot.consecutiveFailures = 1;
        snapshot.observedRateLimitCount = 1;
        snapshot.cooldownUntil = juce::Time::getCurrentTime() + juce::RelativeTime::seconds(30);
    }
    else
    {
        snapshot.healthState = SuiteAiHealthState::degraded;
        snapshot.consecutiveFailures = 1;
    }

    if (const auto* existing = std::find_if(snapshots.begin(), snapshots.end(), [&accountId](const auto& item)
        {
            return item.accountId == accountId;
        }); existing != snapshots.end())
    {
        if (! success)
        {
            snapshot.consecutiveFailures = existing->consecutiveFailures + 1;
            snapshot.observedRateLimitCount = existing->observedRateLimitCount + (rateLimited ? 1 : 0);
            if (! rateLimited && snapshot.consecutiveFailures >= 3)
                snapshot.healthState = SuiteAiHealthState::unavailable;
        }
    }

    upsertHealthSnapshot(snapshots, snapshot);
    if (! store.save(snapshots, errorMessage))
        return false;

    juce::String logError;
    auto logEntry = SuiteLogger::makeEntry("suite-ai",
                                           "health",
                                           success ? "Recorded successful suite AI route outcome."
                                                   : "Recorded failed suite AI route outcome.",
                                           success ? SuiteLogLevel::info
                                                   : (rateLimited ? SuiteLogLevel::warning : SuiteLogLevel::error),
                                           creation::assets::SuiteAppDomain::unknown,
                                           "account=" + accountId + ", latencyMs=" + juce::String(latencyMs)
                                               + ", error=" + errorText,
                                           accountId);
    SuiteLogger::log(logEntry, logError);
    return true;
}

juce::Array<SuiteAiProviderHealthSnapshot> SuiteAiService::loadHealthSnapshots(juce::String& errorMessage) const
{
    return SuiteAiHealthSnapshotStore().load(errorMessage);
}

SuiteAiDiagnosticsLog SuiteAiService::loadDiagnosticsLog(juce::String& errorMessage) const
{
    return SuiteAiDiagnosticsStore().load(errorMessage);
}

juce::Array<SuiteLogEntry> SuiteAiService::loadActivityLog(juce::String& errorMessage, int maxEntries) const
{
    return SuiteLogStore().loadRecent(maxEntries, errorMessage);
}

void SuiteAiService::upsertHealthSnapshot(juce::Array<SuiteAiProviderHealthSnapshot>& snapshots,
                                          const SuiteAiProviderHealthSnapshot& snapshot)
{
    for (auto& existing : snapshots)
    {
        if (existing.accountId == snapshot.accountId)
        {
            existing = snapshot;
            return;
        }
    }

    snapshots.add(snapshot);
}
}
