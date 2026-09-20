#pragma once

#include <creation/assets/ProjectManifest.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/vfs/SuiteVolume.h>

#include <juce_core/juce_core.h>

#include <cstdint>

// The VFS service's storage. EVERYTHING - every project, asset, setting, layout, mapping - lives inside ONE container
// file, vfs.bin, in the VFS root (an exFAT volume in a sparse file; see shared/VFS). Nothing is stored as real files or
// folders on the OS disk. Inside the volume:
//
//   Project Containers/<projectId>/...   one folder per project (the manifest is at ProjectContainerPaths::manifestPath)
//   suite/...                            suite-level entries (settings, layouts, control-surface mappings, ...)
//
// The volume is not thread safe: the service serializes every call to this class with one lock (see Main.cpp).
class VfsProjectStore final
{
public:
    struct ProjectSummary
    {
        juce::String projectId;
        creation::assets::ProjectManifest manifest;
        std::int64_t totalSizeBytes = 0;
    };

    // The size the container is created with when the VFS root has none yet. It is sparse: it takes almost no real
    // space until data is written. The limit is fixed when the container is made.
    static constexpr std::int64_t kContainerSizeBytes = 1024LL * 1024LL * 1024LL * 1024LL;

    // Opens vfs.bin in the settings' VFS root, creating and formatting it the first time. Check isReady().
    explicit VfsProjectStore(const creation::suite::SuiteSettings& settings);

    bool isReady() const noexcept { return volume.isOpen(); }
    const juce::String& startupError() const noexcept { return startupError_; }

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

    bool listProjects(juce::Array<ProjectSummary>& outProjects) const;

    bool cloneProject(const juce::String& sourceProjectId, const juce::String& newProjectName,
                      juce::String& outNewProjectId, juce::String& errorMessage);

    bool deleteProject(const juce::String& projectId, juce::String& errorMessage);

    bool readEntry(const juce::String& projectId, const juce::String& logicalPath,
                   juce::MemoryBlock& outData) const;
    bool writeEntry(const juce::String& projectId, const juce::String& logicalPath,
                    const juce::MemoryBlock& data, juce::String& errorMessage);
    bool removeEntry(const juce::String& projectId, const juce::String& logicalPath);

    // A big entry is written in pieces: each piece lands at `offset` in a partial file, and the last one (offset +
    // chunkSize == totalSize) moves it into place as the finished entry. A finished entry never appears half-written.
    bool writeEntryChunk(const juce::String& projectId, const juce::String& logicalPath,
                         std::int64_t offset, std::int64_t totalSize,
                         const void* chunk, size_t chunkSize,
                         bool& outCompleted, juce::String& errorMessage);
    bool readEntryRange(const juce::String& projectId, const juce::String& logicalPath,
                        std::int64_t offset, std::int64_t length,
                        juce::MemoryBlock& outData, std::int64_t& outTotalSize) const;
    bool discardEntryUpload(const juce::String& projectId, const juce::String& logicalPath);
    juce::StringArray listEntryPaths(const juce::String& projectId) const;

    // Suite-level entries. The logical path starts with "suite/" (the service adds it), e.g. "suite/station-layout.json".
    bool readSuiteEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const;
    bool writeSuiteEntry(const juce::String& logicalPath, const juce::MemoryBlock& data);
    bool removeSuiteEntry(const juce::String& logicalPath);
    juce::StringArray listSuiteEntryPaths() const;

private:
    static juce::String normalizeLogicalPath(const juce::String& logicalPath);
    static bool isSafePath(const juce::String& normalizedPath);
    static juce::String projectFolderPath(const juce::String& projectId);
    bool projectExists(const juce::String& projectId) const;
    static juce::String partialUploadPath(const juce::String& entryPath);

    mutable creation::vfs::SuiteVolume volume;
    juce::String startupError_;
};
