#pragma once

#include <juce_core/juce_core.h>

namespace creation::suite
{
struct SuiteSettings
{
    juce::String suiteVfsRoot;
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
