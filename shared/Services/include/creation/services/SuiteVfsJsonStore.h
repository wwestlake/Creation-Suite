#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
// Factors out the discover()+readEntry()+JSON::parse() / discover()+writeEntry()+JSON::toString()
// boilerplate that every VFS-backed settings domain needs (originally duplicated per-caller in
// SuiteAiSettings.cpp and SuiteDesktopAuthSession.cpp) -- the single place any future settings
// domain, in any app, should go through instead of talking to SuiteVfsServiceClient directly.
class SuiteVfsJsonStore final
{
public:
    // A missing entry is NOT an error -- returns a void var, matching the "nothing saved yet"
    // convention SuiteAiSettings already established. errorMessage is only set when the service
    // is unreachable or the stored entry fails to parse as JSON.
    static juce::var loadJson(const juce::String& logicalPath, juce::String& errorMessage);

    static bool saveJson(const juce::String& logicalPath, const juce::var& value, juce::String& errorMessage);
};
}
