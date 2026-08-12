#include "creation/assets/ProjectSession.h"
#include "creation/services/SuiteVfsServiceClient.h"

namespace
{
juce::String normalizeLogicalPath(const juce::String& logicalPath)
{
    auto normalized = logicalPath.replaceCharacter('\\', '/').trim();
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);
    return normalized;
}
}

namespace creation::assets
{
bool ProjectSession::createNew(const creation::suite::SuiteSettings&,
                               SuiteAppDomain appDomain,
                               const juce::String& projectName,
                               const juce::String& suiteVersion,
                               const juce::String& appVersion,
                               ProjectSession& outSession,
                               juce::String& errorMessage)
{
    if (outSession.isValid())
    {
        errorMessage = "This ProjectSession is already open.";
        return false;
    }

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    return client.createProject(appDomain, projectName, suiteVersion, appVersion,
                                outSession.projectId, outSession.manifest, errorMessage);
}

bool ProjectSession::open(const creation::suite::SuiteSettings&,
                          const juce::String& projectIdToOpen,
                          ProjectSession& outSession,
                          juce::String& errorMessage)
{
    if (outSession.isValid())
    {
        errorMessage = "This ProjectSession is already open.";
        return false;
    }

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    ProjectManifest loadedManifest;
    if (! client.readManifest(projectIdToOpen, loadedManifest))
    {
        errorMessage = "The requested suite project could not be found.";
        return false;
    }

    outSession.projectId = projectIdToOpen;
    outSession.manifest = std::move(loadedManifest);
    return true;
}

bool ProjectSession::peekManifest(const creation::suite::SuiteSettings&,
                                  const juce::String& projectIdToPeek,
                                  ProjectManifest& outManifest,
                                  juce::String& errorMessage)
{
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    if (! client.readManifest(projectIdToPeek, outManifest))
    {
        errorMessage = "The requested suite project could not be found.";
        return false;
    }

    return true;
}

void ProjectSession::close()
{
    // No mount to release -- every operation is already a discrete HTTP round-trip. Kept so
    // existing call sites that pair open()/close() don't need to change.
}

bool ProjectSession::isValid() const noexcept
{
    return projectId.isNotEmpty() && manifest.projectId.isNotEmpty();
}

const juce::String& ProjectSession::getProjectId() const noexcept
{
    return projectId;
}

const ProjectManifest& ProjectSession::getManifest() const noexcept
{
    return manifest;
}

ProjectManifest& ProjectSession::getManifest() noexcept
{
    return manifest;
}

juce::StringArray ProjectSession::listEntryPaths() const
{
    creation::services::SuiteVfsServiceClient client;
    juce::StringArray paths;
    if (client.discover())
        client.listProjectEntries(projectId, paths);
    return paths;
}

bool ProjectSession::containsEntry(const juce::String& logicalPath) const
{
    return listEntryPaths().contains(normalizeLogicalPath(logicalPath));
}

bool ProjectSession::readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
        return false;
    return client.readProjectEntry(projectId, normalizeLogicalPath(logicalPath), outData);
}

bool ProjectSession::writeEntry(const juce::String& logicalPath,
                                const juce::MemoryBlock& data,
                                juce::Time modifiedAt,
                                int compressionLevel)
{
    juce::ignoreUnused(modifiedAt, compressionLevel);

    const auto normalized = normalizeLogicalPath(logicalPath);
    if (normalized.isEmpty() || normalized == ProjectContainerPaths::manifestPath)
        return false;

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
        return false;

    if (! client.writeProjectEntry(projectId, normalized, data))
        return false;

    manifest.modifiedAt = juce::Time::getCurrentTime();
    ++manifest.revision;
    return true;
}

bool ProjectSession::writeEntryFromFile(const juce::String& logicalPath,
                                        const juce::File& sourceFile,
                                        juce::String& errorMessage,
                                        int compressionLevel)
{
    if (! sourceFile.existsAsFile())
    {
        errorMessage = "The source file for the project entry does not exist.";
        return false;
    }

    juce::MemoryBlock data;
    if (! sourceFile.loadFileAsData(data))
    {
        errorMessage = "Could not load the source file into memory for upload.";
        return false;
    }

    if (! writeEntry(logicalPath, data, sourceFile.getLastModificationTime(), compressionLevel))
    {
        errorMessage = "Could not write the entry into the project.";
        return false;
    }

    return true;
}

bool ProjectSession::removeEntry(const juce::String& logicalPath)
{
    const auto normalized = normalizeLogicalPath(logicalPath);

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
        return false;
    if (! client.removeProjectEntry(projectId, normalized))
        return false;

    manifest.modifiedAt = juce::Time::getCurrentTime();
    ++manifest.revision;
    return true;
}

void ProjectSession::upsertAssetDescriptor(const AssetDescriptor& descriptor)
{
    for (auto& asset : manifest.assetCatalog.assets)
    {
        if (asset.versionId == descriptor.versionId || (! descriptor.id.isEmpty() && asset.id == descriptor.id && asset.logicalPath == descriptor.logicalPath))
        {
            asset = descriptor;
            manifest.modifiedAt = juce::Time::getCurrentTime();
            return;
        }
    }

    manifest.assetCatalog.assets.add(descriptor);
    manifest.modifiedAt = juce::Time::getCurrentTime();
}

bool ProjectSession::removeAssetDescriptorByVersionId(const AssetVersionId& versionId)
{
    for (int index = 0; index < manifest.assetCatalog.assets.size(); ++index)
    {
        if (manifest.assetCatalog.assets.getReference(index).versionId == versionId)
        {
            manifest.assetCatalog.assets.remove(index);
            manifest.modifiedAt = juce::Time::getCurrentTime();
            return true;
        }
    }

    return false;
}

bool ProjectSession::materializeEntry(const creation::suite::SuiteSettings& settings,
                                      const juce::String& logicalPath,
                                      MaterializationAccess access,
                                      MaterializedAssetLease& outLease,
                                      juce::String& errorMessage) const
{
    if (! isValid())
    {
        errorMessage = "The project session is not valid.";
        return false;
    }

    return AssetMaterializer::materializeEntry(settings,
                                               projectId,
                                               logicalPath,
                                               access,
                                               outLease,
                                               errorMessage);
}

bool ProjectSession::reconcileMaterializedEntry(const creation::suite::SuiteSettings& settings,
                                                MaterializedAssetLease& lease,
                                                juce::String& errorMessage,
                                                bool releaseLeaseAfterCommit)
{
    juce::ignoreUnused(settings);

    if (! lease.isValid())
    {
        errorMessage = "The materialized asset lease is not valid.";
        return false;
    }

    if (lease.projectId != manifest.projectId)
    {
        errorMessage = "The materialized lease belongs to a different project session.";
        return false;
    }

    if (lease.access != MaterializationAccess::writable)
    {
        errorMessage = "This materialized lease was opened as read-only and cannot be reconciled.";
        return false;
    }

    if (! AssetMaterializer::hasLocalChanges(lease))
    {
        if (releaseLeaseAfterCommit)
            return AssetMaterializer::releaseLease(lease, errorMessage);
        return true;
    }

    if (! writeEntryFromFile(lease.logicalPath, lease.materializedFile, errorMessage))
        return false;

    for (auto& descriptor : manifest.assetCatalog.assets)
    {
        if (normalizeLogicalPath(descriptor.logicalPath) == normalizeLogicalPath(lease.logicalPath))
        {
            descriptor.fileSizeBytes = static_cast<std::uint64_t>(lease.materializedFile.getSize());
            descriptor.modifiedAt = juce::Time::getCurrentTime();
            ++descriptor.revision;
            if (descriptor.version.isEmpty())
                descriptor.version = "1";
            break;
        }
    }

    if (! commit(errorMessage))
        return false;

    if (releaseLeaseAfterCommit)
        return AssetMaterializer::releaseLease(lease, errorMessage);

    return true;
}

bool ProjectSession::commit(juce::String& errorMessage)
{
    if (! isValid())
    {
        errorMessage = "The project session is not valid.";
        return false;
    }

    manifest.modifiedAt = juce::Time::getCurrentTime();

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    if (! client.writeManifest(projectId, manifest))
    {
        errorMessage = "Could not save the project manifest.";
        return false;
    }

    return true;
}
}
