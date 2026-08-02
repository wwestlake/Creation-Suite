#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/AssetCatalog.h"
#include "creation/assets/AssetMaterializer.h"
#include "creation/assets/ProjectManifest.h"
#include "creation/suite/SuiteStoragePaths.h"
#include "creation/vfs/SuiteVolume.h"

namespace creation::assets
{
// A live handle on one suite project: a single real container file on disk,
// mounted as a FatFs volume for the lifetime of this session. Asset writes
// go straight to the mounted volume; commit() is the explicit save point
// that persists the project manifest (catalog, revision, tags) into the
// container.
//
// Not copyable -- the mounted volume owns an exclusive drive slot and a real
// file handle, so a ProjectSession is either default-constructed and then
// populated in place by createNew()/open(), or moved.
class ProjectSession final
{
public:
    ProjectSession() = default;

    static bool createNew(const creation::suite::SuiteSettings& settings,
                          SuiteAppDomain appDomain,
                          const juce::String& projectName,
                          const juce::String& suiteVersion,
                          const juce::String& appVersion,
                          ProjectSession& outSession,
                          juce::String& errorMessage);

    static bool open(const juce::File& containerFile,
                     ProjectSession& outSession,
                     juce::String& errorMessage);

    // Reads just the manifest out of a container without keeping it mounted
    // -- used for listing/browsing projects without holding their volumes open.
    static bool peekManifest(const juce::File& containerFile,
                             ProjectManifest& outManifest,
                             juce::String& errorMessage);

    // Releases the mounted volume so another ProjectSession (in this process
    // or another) can open the same container file. Safe to call when not open.
    void close();

    bool isValid() const noexcept;
    const juce::File& getContainerFile() const noexcept;
    const ProjectManifest& getManifest() const noexcept;
    ProjectManifest& getManifest() noexcept;

    // Every asset's logical path in the container (excludes the manifest itself).
    juce::StringArray listEntryPaths() const;

    bool containsEntry(const juce::String& logicalPath) const;
    bool readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const;
    bool writeEntry(const juce::String& logicalPath,
                    const juce::MemoryBlock& data,
                    juce::Time modifiedAt = juce::Time::getCurrentTime(),
                    int compressionLevel = 9);
    bool writeEntryFromFile(const juce::String& logicalPath,
                            const juce::File& sourceFile,
                            juce::String& errorMessage,
                            int compressionLevel = 9);
    bool removeEntry(const juce::String& logicalPath);

    void upsertAssetDescriptor(const AssetDescriptor& descriptor);
    bool removeAssetDescriptorByVersionId(const AssetVersionId& versionId);

    bool materializeEntry(const creation::suite::SuiteSettings& settings,
                          const juce::String& logicalPath,
                          MaterializationAccess access,
                          MaterializedAssetLease& outLease,
                          juce::String& errorMessage) const;
    bool reconcileMaterializedEntry(const creation::suite::SuiteSettings& settings,
                                    MaterializedAssetLease& lease,
                                    juce::String& errorMessage,
                                    bool releaseLeaseAfterCommit = true);

    // Persists the manifest (asset catalog, revision, tags) into the
    // container. Asset writes/removals themselves are already durable on
    // the mounted volume by the time writeEntry/removeEntry return.
    bool commit(juce::String& errorMessage);

private:
    static juce::String normalizeLogicalPath(const juce::String& logicalPath);

    juce::File containerFile;
    ProjectManifest manifest;
    creation::vfs::SuiteVolume volume;
};
}
