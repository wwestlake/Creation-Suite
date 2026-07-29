#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
class SuiteEula final
{
public:
    static juce::String currentVersion();
    static juce::String currentText();
};
}
