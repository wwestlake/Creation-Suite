#pragma once

#include <creation/services/SuiteLogging.h>
#include <creation/services/SuiteAiOrchestration.h>

namespace creation::services
{
struct SuiteAiDiagnosticsLog
{
    juce::Array<SuiteAiDiagnosticsEvent> events;
};

class SuiteAiHealthSnapshotStore final
{
public:
    juce::Array<SuiteAiProviderHealthSnapshot> load(juce::String& errorMessage) const;
    bool save(const juce::Array<SuiteAiProviderHealthSnapshot>& snapshots, juce::String& errorMessage) const;

    juce::File getSettingsFile() const;
};

class SuiteAiDiagnosticsStore final
{
public:
    SuiteAiDiagnosticsLog load(juce::String& errorMessage) const;
    bool save(const SuiteAiDiagnosticsLog& log, juce::String& errorMessage) const;
    bool append(const SuiteAiDiagnosticsEvent& event,
                int maxEventsToKeep,
                juce::String& errorMessage) const;

    juce::File getSettingsFile() const;
};

class SuiteAiService final
{
public:
    struct RoutePlanningResult
    {
        SuiteAiSettings settings;
        juce::Array<SuiteAiProviderHealthSnapshot> healthSnapshots;
        SuiteAiRoutingDecision routingDecision;
    };

    RoutePlanningResult planRequest(const SuiteAiRequestDescriptor& request,
                                    juce::String& errorMessage) const;

    bool recordDiagnosticsEvent(const SuiteAiDiagnosticsEvent& event,
                                juce::String& errorMessage,
                                int maxEventsToKeep = 200) const;

    bool recordRouteOutcome(const juce::String& accountId,
                            bool success,
                            bool rateLimited,
                            juce::int64 latencyMs,
                            const juce::String& errorText,
                            juce::String& errorMessage) const;

    juce::Array<SuiteAiProviderHealthSnapshot> loadHealthSnapshots(juce::String& errorMessage) const;
    SuiteAiDiagnosticsLog loadDiagnosticsLog(juce::String& errorMessage) const;
    juce::Array<SuiteLogEntry> loadActivityLog(juce::String& errorMessage, int maxEntries = 100) const;

private:
    static void upsertHealthSnapshot(juce::Array<SuiteAiProviderHealthSnapshot>& snapshots,
                                     const SuiteAiProviderHealthSnapshot& snapshot);
};
}
