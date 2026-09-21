#pragma once

#include <juce_core/juce_core.h>

namespace creation::suite
{
struct SuiteSettings
{
    juce::String suiteVfsRoot;

    // Directory holding the suite's built executables (CreationSuiteVfsService.exe
    // and friends) -- the one piece of information an outside process (an
    // external Python tool, a Blender add-on) needs to launch a suite service
    // itself when discovery finds nothing running. Read by
    // SuiteVfsServiceClient::discover() instead of a hardcoded dev-tree path.
    juce::String suiteExecutablesRoot;
};

// The name of the suite's storage service, which every suite app needs in order to run.
constexpr const char* vfsServiceExecutableName = "DjehutiSuiteVfsService.exe";

// Where the installer put the suite's shared services: the folder it recorded in the registry, else the default install
// folder. An empty File if the service is in neither, so a caller can say plainly that it is not installed.
juce::File installedServicesDirectory();

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
