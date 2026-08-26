#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/AssetCatalog.h"
#include "creation/assets/AssetMaterializer.h"
#include "creation/assets/ProjectManifest.h"
#include "creation/suite/SuiteStoragePaths.h"

namespace creation::assets
{
// A live handle on one suite project -- a real folder the VFS service owns, addressed only
// by projectId. Never a real filesystem path: this session is an HTTP client of the suite
// VFS service (creation::services::SuiteVfsServiceClient), same as every other suite app --
// see docs/architecture/Suite-Shared-Project-Model.md. Asset writes/removes are durable on
// the service the instant they return; commit() persists the in-memory manifest (catalog,
// revision, tags), which IS still legitimately batched in memory between commits.
class ProjectSession final
{
public:
    ProjectSession() = default;

    // outSession may already hold a different project -- there is nothing to release (see
    // close() below), so createNew simply overwrites it with the new project. Exactly one
    // project can be the active target of a given ProjectSession at a time; switching targets
    // is just reassignment, not an error.
    static bool createNew(const creation::suite::SuiteSettings& settings,
                          SuiteAppDomain appDomain,
                          const juce::String& projectName,
                          const juce::String& suiteVersion,
                          const juce::String& appVersion,
                          ProjectSession& outSession,
                          juce::String& errorMessage);

    // Same reassignment behavior as createNew: opening a different project while one is
    // already held simply switches outSession to point at the new project.
    static bool open(const creation::suite::SuiteSettings& settings,
                     const juce::String& projectId,
                     ProjectSession& outSession,
                     juce::String& errorMessage);

    // Reads just the manifest for a project without holding a session open for it --
    // used for listing/browsing projects.
    static bool peekManifest(const creation::suite::SuiteSettings& settings,
                             const juce::String& projectId,
                             ProjectManifest& outManifest,
                             juce::String& errorMessage);

    // No-op, kept for call-site compatibility: there is no mount to release anymore, every
    // operation is a discrete HTTP round-trip to the VFS service.
    void close();

    bool isValid() const noexcept;
    const juce::String& getProjectId() const noexcept;
    const ProjectManifest& getManifest() const noexcept;
    ProjectManifest& getManifest() noexcept;

    // Every asset's logical path in the project (excludes the manifest itself).
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

    // Persists the manifest (asset catalog, revision, tags). Asset writes/removals themselves
    // are already durable on the VFS service by the time writeEntry/removeEntry return.
    bool commit(juce::String& errorMessage);

private:
    juce::String projectId;
    ProjectManifest manifest;
};
}
