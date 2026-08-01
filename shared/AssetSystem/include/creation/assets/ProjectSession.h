#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/AssetCatalog.h"
#include "creation/assets/AssetMaterializer.h"
#include "creation/assets/ProjectContainerIO.h"
#include "creation/suite/SuiteStoragePaths.h"

namespace creation::assets
{
class ProjectSession final
{
public:
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

    bool isValid() const noexcept;
    const juce::File& getContainerFile() const noexcept;
    const ProjectManifest& getManifest() const noexcept;
    ProjectManifest& getManifest() noexcept;
    const juce::Array<ProjectContainerEntry>& getEntries() const noexcept;

    bool containsEntry(const juce::String& logicalPath) const noexcept;
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

    bool hasExternalChanges(juce::String& errorMessage) const;
    bool reloadFromDisk(juce::String& errorMessage);
    bool commit(juce::String& errorMessage);

private:
    static juce::String normalizeLogicalPath(const juce::String& logicalPath);
    int findEntryIndex(const juce::String& logicalPath) const noexcept;

    juce::File containerFile;
    ProjectManifest manifest;
    juce::Array<ProjectContainerEntry> entries;
    int baselineRevision = -1;
};
}
