#pragma once

#include <creation/assets/ProjectManifest.h>

namespace creation::services
{
enum class SuiteLogLevel
{
    debug,
    info,
    warning,
    error
};

struct SuiteLogEntry
{
    juce::Time timestamp;
    juce::String subsystem;
    juce::String category;
    juce::String message;
    juce::String details;
    juce::String correlationId;
    creation::assets::SuiteAppDomain appDomain = creation::assets::SuiteAppDomain::unknown;
    SuiteLogLevel level = SuiteLogLevel::info;
};

class SuiteLogStore final
{
public:
    juce::Array<SuiteLogEntry> load(juce::String& errorMessage) const;
    juce::Array<SuiteLogEntry> loadRecent(int maxEntries, juce::String& errorMessage) const;
    bool save(const juce::Array<SuiteLogEntry>& entries, juce::String& errorMessage) const;
    bool append(const SuiteLogEntry& entry, juce::String& errorMessage, int maxEntriesToKeep = 1000) const;

    juce::File getSettingsFile() const;
};

class SuiteLogger final
{
public:
    static SuiteLogEntry makeEntry(const juce::String& subsystem,
                                   const juce::String& category,
                                   const juce::String& message,
                                   SuiteLogLevel level = SuiteLogLevel::info,
                                   creation::assets::SuiteAppDomain appDomain = creation::assets::SuiteAppDomain::unknown,
                                   const juce::String& details = {},
                                   const juce::String& correlationId = {});

    static bool log(const SuiteLogEntry& entry,
                    juce::String& errorMessage,
                    int maxEntriesToKeep = 1000);
};
}
