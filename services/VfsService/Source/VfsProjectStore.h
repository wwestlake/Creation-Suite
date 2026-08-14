#pragma once

#include <creation/assets/ProjectManifest.h>
#include <creation/suite/SuiteSettings.h>

#include <juce_core/juce_core.h>

// Real, plain juce::File folder I/O against project directories under the configured VFS
// root -- no FatFs, no mounted volume, no packed container file. Deliberately private to
// this service executable (not a shared/linkable library): the whole point of the "service
// owns the entire VFS" architecture is that no app process ever touches VFS files directly,
// and keeping the implementation un-linkable from anywhere else makes that structural, not
// just a documented convention. See docs/architecture/Suite-Shared-Project-Model.md.
//
// One project = one real folder:
//   <suiteVfsRoot>/Project Containers/<AppDomain>/<sanitized-name>/
//     Project/project-manifest.json   (creation::assets::ProjectContainerPaths::manifestPath)
//     Assets/..., Metadata/..., Exports/...   (whatever entries the app writes)
// The suite root project (small suite-wide settings entries) is just another instance of
// this, rooted directly at <suiteVfsRoot>/suite/ -- see resolveSuiteRootFolder.
//
// Every public method here is a discrete, stateless filesystem operation; callers
// (Main.cpp's HTTP handlers) are responsible for their own request-level locking.
class VfsProjectStore final
{
public:
    struct ProjectSummary
    {
        juce::String projectId;
        creation::assets::ProjectManifest manifest;
        // The old packed-container model got "how big is this project" for free from the OS
        // (one file's size). A real folder tree doesn't have that for free -- computed here by
        // summing every entry's size, so callers (project browser UI) don't have to.
        std::int64_t totalSizeBytes = 0;
    };

    explicit VfsProjectStore(const creation::suite::SuiteSettings& settings);

    // The root the suite-entry methods below resolve paths against. Callers already prefix
    // logical paths with "suite/" (Main.cpp's normalizeEntryPath), so this is the VFS root
    // itself -- the prefix in the path is what puts entries under <suiteVfsRoot>/suite/ on
    // disk, matching the layout the old container used. Used for the existing /suite/entry*
    // endpoints, reimplemented against this store instead of a mounted volume.
    juce::File suiteRootFolder() const;

    bool createProject(creation::assets::SuiteAppDomain appDomain,
                       const juce::String& projectName,
                       const juce::String& suiteVersion,
                       const juce::String& appVersion,
                       juce::String& outProjectId,
                       creation::assets::ProjectManifest& outManifest,
                       juce::String& errorMessage);

    bool readManifest(const juce::String& projectId, creation::assets::ProjectManifest& outManifest,
                      juce::String& errorMessage) const;
    bool writeManifest(const juce::String& projectId, const creation::assets::ProjectManifest& manifest,
                       juce::String& errorMessage);

    bool listProjects(creation::assets::SuiteAppDomain appDomain, juce::Array<ProjectSummary>& outProjects) const;
    bool findProjectFolderById(const juce::String& projectId, juce::File& outFolder) const;

    bool cloneProject(const juce::String& sourceProjectId, const juce::String& newProjectName,
                      juce::String& outNewProjectId, juce::String& errorMessage);

    bool deleteProject(const juce::String& projectId, juce::String& errorMessage);

    bool readEntry(const juce::String& projectId, const juce::String& logicalPath,
                   juce::MemoryBlock& outData) const;
    bool writeEntry(const juce::String& projectId, const juce::String& logicalPath,
                    const juce::MemoryBlock& data, juce::String& errorMessage);
    bool removeEntry(const juce::String& projectId, const juce::String& logicalPath);
    juce::StringArray listEntryPaths(const juce::String& projectId) const;

    // Suite-root-scoped equivalents (no projectId -- always the one suite folder).
    bool readSuiteEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const;
    bool writeSuiteEntry(const juce::String& logicalPath, const juce::MemoryBlock& data);
    bool removeSuiteEntry(const juce::String& logicalPath);
    juce::StringArray listSuiteEntryPaths() const;

private:
    static juce::String normalizeLogicalPath(const juce::String& logicalPath);
    static bool readManifestFromFolder(const juce::File& projectFolder, creation::assets::ProjectManifest& outManifest,
                                       juce::String& errorMessage);
    static bool writeManifestToFolder(const juce::File& projectFolder, const creation::assets::ProjectManifest& manifest,
                                      juce::String& errorMessage);
    static bool readEntryFromFolder(const juce::File& rootFolder, const juce::String& logicalPath,
                                    juce::MemoryBlock& outData);
    static bool writeEntryToFolder(const juce::File& rootFolder, const juce::String& logicalPath,
                                   const juce::MemoryBlock& data, juce::String& errorMessage);
    static bool removeEntryFromFolder(const juce::File& rootFolder, const juce::String& logicalPath);
    static juce::StringArray listEntryPathsInFolder(const juce::File& rootFolder, const juce::String& excludePath);

    creation::suite::SuiteSettings settings_;
};
