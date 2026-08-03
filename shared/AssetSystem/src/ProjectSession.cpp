#include "creation/assets/ProjectSession.h"

namespace
{
// Reserved logical size for a freshly created container. The backing file
// is an NTFS sparse file, so this costs no real disk space up front --
// FatFs just needs a fixed volume size to format against. Containers are
// formatted exFAT (VFS-M3), which has no meaningful per-file size cap, so
// this is sized for real project assets (multi-GB video/audio) rather than
// FAT32's old 4GB ceiling.
constexpr juce::int64 kDefaultContainerSizeBytes = 64LL * 1024 * 1024 * 1024;
}

namespace creation::assets
{
juce::String ProjectSession::normalizeLogicalPath(const juce::String& logicalPath)
{
    auto normalized = logicalPath.replaceCharacter('\\', '/').trim();
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);
    return normalized;
}

bool ProjectSession::createNew(const creation::suite::SuiteSettings& settings,
                               SuiteAppDomain appDomain,
                               const juce::String& projectName,
                               const juce::String& suiteVersion,
                               const juce::String& appVersion,
                               ProjectSession& outSession,
                               juce::String& errorMessage)
{
    if (outSession.volume.isOpen())
    {
        errorMessage = "This ProjectSession is already open.";
        return false;
    }

    const auto containerPath = creation::suite::getProjectContainerPath(settings, appDomain, projectName);
    const auto parentDirectory = containerPath.getParentDirectory();
    if (! parentDirectory.exists() && ! parentDirectory.createDirectory())
    {
        errorMessage = "Could not create the target project container directory.";
        return false;
    }

    if (! outSession.volume.createAndFormat(containerPath, kDefaultContainerSizeBytes, errorMessage))
        return false;

    outSession.containerFile = containerPath;
    outSession.manifest = creation::suite::createDefaultManifest(projectName, appDomain, suiteVersion, appVersion);
    return true;
}

bool ProjectSession::open(const juce::File& containerFileToOpen,
                          ProjectSession& outSession,
                          juce::String& errorMessage)
{
    if (outSession.volume.isOpen())
    {
        errorMessage = "This ProjectSession is already open.";
        return false;
    }

    if (! outSession.volume.open(containerFileToOpen, errorMessage))
        return false;

    juce::MemoryBlock manifestData;
    if (! outSession.volume.readFile(ProjectContainerPaths::manifestPath, manifestData, errorMessage))
    {
        errorMessage = "The suite project container does not contain a project manifest.";
        outSession.volume.close();
        return false;
    }

    const auto manifestText = juce::String::fromUTF8(static_cast<const char*>(manifestData.getData()),
                                                      static_cast<int>(manifestData.getSize()));
    ProjectManifest loadedManifest;
    if (! deserializeManifest(manifestText, loadedManifest, errorMessage))
    {
        outSession.volume.close();
        return false;
    }

    outSession.containerFile = containerFileToOpen;
    outSession.manifest = std::move(loadedManifest);
    return true;
}

bool ProjectSession::peekManifest(const juce::File& containerFile,
                                  ProjectManifest& outManifest,
                                  juce::String& errorMessage)
{
    creation::vfs::SuiteVolume volume;
    if (! volume.open(containerFile, errorMessage))
        return false;

    juce::MemoryBlock manifestData;
    if (! volume.readFile(ProjectContainerPaths::manifestPath, manifestData, errorMessage))
    {
        errorMessage = "The suite project container does not contain a project manifest.";
        return false;
    }

    const auto manifestText = juce::String::fromUTF8(static_cast<const char*>(manifestData.getData()),
                                                      static_cast<int>(manifestData.getSize()));
    return deserializeManifest(manifestText, outManifest, errorMessage);
}

void ProjectSession::close()
{
    volume.close();
}

bool ProjectSession::isValid() const noexcept
{
    return volume.isOpen() && manifest.projectId.isNotEmpty();
}

const juce::File& ProjectSession::getContainerFile() const noexcept
{
    return containerFile;
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
    auto paths = volume.listFiles();
    paths.removeString(ProjectContainerPaths::manifestPath);
    return paths;
}

bool ProjectSession::containsEntry(const juce::String& logicalPath) const
{
    return volume.fileExists(normalizeLogicalPath(logicalPath));
}

bool ProjectSession::readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    juce::String errorMessage;
    return volume.readFile(normalizeLogicalPath(logicalPath), outData, errorMessage);
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

    juce::String errorMessage;
    if (! volume.writeFile(normalized, data, errorMessage))
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
        errorMessage = "The source file for the project container entry does not exist.";
        return false;
    }

    juce::MemoryBlock data;
    if (! sourceFile.loadFileAsData(data))
    {
        errorMessage = "Could not load the source file into memory for container staging.";
        return false;
    }

    if (! writeEntry(logicalPath, data, sourceFile.getLastModificationTime(), compressionLevel))
    {
        errorMessage = "Could not write the entry into the project container.";
        return false;
    }

    return true;
}

bool ProjectSession::removeEntry(const juce::String& logicalPath)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (! volume.fileExists(normalized))
        return false;

    juce::String errorMessage;
    if (! volume.deleteFile(normalized, errorMessage))
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
                                               volume,
                                               manifest.projectId,
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
    const auto manifestText = serializeManifest(manifest, true);
    const juce::MemoryBlock manifestData(manifestText.toRawUTF8(), manifestText.getNumBytesAsUTF8());

    return volume.writeFile(ProjectContainerPaths::manifestPath, manifestData, errorMessage);
}
}
