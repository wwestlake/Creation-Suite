#pragma once

#include <juce_core/juce_core.h>

namespace creation::suite
{
struct SuiteSettings
{
    juce::String suiteVfsRoot;
    juce::String sharedResourcesRoot;
    juce::String creationStationProjectsRoot;
    juce::String creationEngineProjectsRoot;
    juce::String creationMovieProjectsRoot;
    juce::String creationLiveProjectsRoot;
};

class SuiteSettingsStore final
{
public:
    SuiteSettingsStore();

    SuiteSettings load(juce::String& errorMessage) const;
    bool save(const SuiteSettings& settings, juce::String& errorMessage) const;

    juce::File getSuiteConfigDirectory() const;
    juce::File getSuiteSettingsFile() const;

private:
    SuiteSettings makeDefaultSettings() const;
};
}
