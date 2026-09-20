#pragma once

#include <creation/assets/ProjectSession.h>
#include <creation/frust/FratePodVfsResolver.h>

#include <frate/PodArchive.h>
#include <frate/PodBuild.h>

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
    bool success = false;
    juce::String output;          // the compiler's diagnostics, one per line, or why the build could not start
    juce::String sourceRoot;      // where the pod's source lives in the project VFS
    juce::String derivedRoot;     // where its build output lives in the project VFS
    juce::String objectEntry;     // the object file, in the project VFS
    juce::String packageEntry;    // the .frpod, in the project VFS
    frate::PodFiles files;        // the pod that was built, held in memory
};

class SuiteFratePodWorkspace;

// Pods a build depends on: the pods authored in this project first, then the
// Suite's pod cache and the registry, both through the VFS. All in memory.
class SuitePodSource final : public frate::PodSource {
public:
    SuitePodSource(creation::assets::ProjectSession& session, FratePodVfsResolver& resolver);

    bool findPod(const std::string& name, const std::string& version, frate::PodFiles& files) override;

private:
    creation::assets::ProjectSession& session_;
    FratePodVfsResolver& resolver_;
};

// Owns the complete Suite pod lifecycle. Source and build output live in the
// project's VFS and nowhere else: a build reads the pod's entries into memory,
// compiles them in memory (frate::buildPod), and writes the object file and the
// .frpod package back as VFS entries. No folder is created, no temp file is
// written, and no other program is started.
class SuiteFratePodWorkspace final {
public:
    SuiteFratePodWorkspace(creation::assets::ProjectSession& session, frate::PodSource& dependencies);

    bool scaffold(const PodScaffoldOptions& options, juce::String& error);
    bool writeSource(const PodScaffoldOptions& options, const juce::String& source, juce::String& error);

    // The pod's files, read from the project VFS into memory.
    bool readPod(const juce::String& podName, frate::PodFiles& files, juce::String& error) const;

    PodWorkspaceBuildResult build(const juce::String& podName);

    static juce::String sourceRootFor(const juce::String& podName);
    static juce::String derivedRootFor(const juce::String& podName);

private:
    creation::assets::ProjectSession& session_;
    frate::PodSource& dependencies_;

    static bool validPodName(const juce::String& name);
};

} // namespace creation::frust
