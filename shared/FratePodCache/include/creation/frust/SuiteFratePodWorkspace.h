#pragma once

#include <creation/assets/ProjectSession.h>
#include <creation/frust/SuiteFrateBuildService.h>
#include <juce_core/juce_core.h>

namespace creation::frust {

struct PodScaffoldOptions {
    juce::String name;
    juce::String version { "0.1.0" };
    juce::String description { "A Creation Suite FRust pod." };
    juce::String intendedApplication { "creation-suite" };
    bool exposeAsNodeLibrary = true;
};

struct PodWorkspaceBuildResult {
    BuildResult build;
    juce::String sourceRoot;
    juce::String derivedRoot;
    juce::String packageEntry;
    juce::File materializedPodDirectory;
};

// Owns the complete Suite pod lifecycle. Project source and derived output
// remain authoritative in VFS; the filesystem tree exists only as a
// disposable bridge to the unmodified frate CLI/compiler toolchain.
class SuiteFratePodWorkspace final {
public:
    SuiteFratePodWorkspace(creation::assets::ProjectSession& session,
                           SuiteFrateBuildService& buildService,
                           juce::File materializationRoot);

    bool scaffold(const PodScaffoldOptions& options, juce::String& error);
    bool writeSource(const PodScaffoldOptions& options, const juce::String& source, juce::String& error);
    PodWorkspaceBuildResult build(const juce::String& podName);

    static juce::String sourceRootFor(const juce::String& podName);
    static juce::String derivedRootFor(const juce::String& podName);

private:
    creation::assets::ProjectSession& session_;
    SuiteFrateBuildService& buildService_;
    juce::File materializationRoot_;

    static bool validPodName(const juce::String& name);
    bool materialize(const juce::String& podName, juce::File& outDirectory, juce::String& error);
    bool persistDerived(const juce::String& podName, const juce::File& podDirectory,
                        juce::String& packageEntry, juce::String& error);
};

} // namespace creation::frust
