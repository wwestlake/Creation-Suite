#pragma once

// The one thing a Suite app calls to use FRust and Frate. The compiler and Frate are
// libraries linked into the app; SuiteFrust gives them the VFS as their file system and
// the VFS as their log (see VfsEnvironment.h). No app starts a compiler or `frate`, and
// nothing here touches the operating system's file system.

#include <creation/assets/ProjectSession.h>
#include <creation/frust/PluginRuntime.h>
#include <creation/frust/VfsEnvironment.h>
#include <creation/services/SuiteVfsServiceClient.h>

#include <frate/FrateRegistryClient.h>

#include <juce_core/juce_core.h>

namespace creation::frust {

struct PodScaffoldOptions {
    juce::String name;
    juce::String version { "0.1.0" };
    juce::String description { "A Creation Suite FRust pod." };
    juce::String intendedApplication { "creation-suite" };
    bool exposeAsNodeLibrary = true;
};

struct FrustOutcome {
    bool ok = false;
    juce::String output;   // diagnostics or a short result line, in words a user can read
};

class SuiteFrust final {
public:
    SuiteFrust(creation::assets::ProjectSession& project,
               creation::services::SuiteVfsServiceClient& suiteVfs,
               frate::FrateRegistryClient& registry);

    // ---- Loose source (the Script panel) ----
    // Stores `text` in the project VFS at `virtualPath` and compiles it from there (check only).
    FrustOutcome checkSource(const juce::String& virtualPath, const juce::String& text);

    // ---- Pods authored in this project ----
    static juce::String podRootFor(const juce::String& podName);
    static bool validPodName(const juce::String& name);

    bool scaffoldPod(const PodScaffoldOptions& options, juce::String& error);
    bool writePodSource(const PodScaffoldOptions& options, const juce::String& source, juce::String& error);
    FrustOutcome buildPod(const juce::String& podName);      // writes <podRoot>/build/<name>.o
    FrustOutcome packagePod(const juce::String& podName);    // writes <podRoot>/dist/<name>-<version>.frpod

    // ---- Registry pods ----
    // Makes sure <name> <version> and its dependencies are in the Suite pod store; downloads each
    // missing one straight into memory and installs it there. Nothing touches the disk.
    bool installRegistryPod(const juce::String& name, const juce::String& version, juce::String& error);

    // ---- Running ----
    bool loadAuthoredPod(PluginRuntime& runtime, const juce::String& key, const juce::String& podName, juce::String& error);
    bool loadInstalledPod(PluginRuntime& runtime, const juce::String& key, const juce::String& name,
                          const juce::String& version, juce::String& error);

    // Writes any pending log lines to the project's VFS log. Every operation above does this itself;
    // call it only after using environment() directly.
    void flushLog() { log_.flush(); }

    ::frust::HostEnvironment environment() { return ::frust::HostEnvironment{ files_, log_ }; }

private:
    bool installRegistryPodRecursive(const juce::String& name, const juce::String& version, int depth, juce::String& error);
    bool loadEntry(PluginRuntime& runtime, const juce::String& key, const juce::String& podRoot,
                   const std::string& podsRoot, juce::String& error);

    creation::assets::ProjectSession& project_;
    frate::FrateRegistryClient& registry_;
    VfsFileSystem files_;
    VfsLogSink log_;
};

} // namespace creation::frust
