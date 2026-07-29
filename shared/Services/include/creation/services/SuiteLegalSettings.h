#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
struct SuiteLegalSettings
{
    bool eulaAccepted = false;
    juce::String acceptedEulaVersion;
    juce::Time acceptedAt;
};

class SuiteLegalSettingsStore final
{
public:
    SuiteLegalSettings load(juce::String& errorMessage) const;
    bool save(const SuiteLegalSettings& settings, juce::String& errorMessage) const;

    juce::File getSettingsFile() const;
};
}
