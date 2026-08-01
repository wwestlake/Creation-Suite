#include "creation/assets/ProjectSession.h"

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
    juce::ignoreUnused(errorMessage);

    outSession.containerFile = creation::suite::getProjectContainerPath(settings, appDomain, projectName);
    outSession.manifest = creation::suite::createDefaultManifest(projectName, appDomain, suiteVersion, appVersion);
    outSession.entries.clearQuick();
    outSession.baselineRevision = -1;
    return true;
}

bool ProjectSession::open(const juce::File& containerFileToOpen,
                          ProjectSession& outSession,
                          juce::String& errorMessage)
{
    ProjectManifest loadedManifest;
    juce::Array<ProjectContainerEntry> loadedEntries;
    if (! ProjectContainerIO::readContainer(containerFileToOpen, loadedManifest, loadedEntries, errorMessage))
        return false;

    outSession.containerFile = containerFileToOpen;
    outSession.manifest = std::move(loadedManifest);
    outSession.entries = std::move(loadedEntries);
    outSession.baselineRevision = outSession.manifest.revision;
    return true;
}

bool ProjectSession::isValid() const noexcept
{
    return containerFile != juce::File() && manifest.projectId.isNotEmpty();
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

const juce::Array<ProjectContainerEntry>& ProjectSession::getEntries() const noexcept
{
    return entries;
}

int ProjectSession::findEntryIndex(const juce::String& logicalPath) const noexcept
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    for (int index = 0; index < entries.size(); ++index)
        if (entries.getReference(index).logicalPath == normalized)
            return index;

    return -1;
}

bool ProjectSession::containsEntry(const juce::String& logicalPath) const noexcept
{
    return findEntryIndex(logicalPath) >= 0;
}

bool ProjectSession::readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    const auto index = findEntryIndex(logicalPath);
    if (index < 0)
        return false;

    outData = entries.getReference(index).data;
    return true;
}

bool ProjectSession::writeEntry(const juce::String& logicalPath,
                                const juce::MemoryBlock& data,
                                juce::Time modifiedAt,
                                int compressionLevel)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (normalized.isEmpty() || normalized == ProjectContainerPaths::manifestPath)
        return false;

    ProjectContainerEntry entry;
    entry.logicalPath = normalized;
    entry.data = data;
    entry.modifiedAt = modifiedAt;
    entry.compressionLevel = juce::jlimit(0, 9, compressionLevel);

    const auto existingIndex = findEntryIndex(normalized);
    if (existingIndex >= 0)
        entries.getReference(existingIndex) = std::move(entry);
    else
        entries.add(std::move(entry));

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
        errorMessage = "Could not stage the entry into the project session.";
        return false;
    }

    return true;
}

bool ProjectSession::removeEntry(const juce::String& logicalPath)
{
    const auto existingIndex = findEntryIndex(logicalPath);
    if (existingIndex < 0)
        return false;

    entries.remove(existingIndex);
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
                                               containerFile,
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

bool ProjectSession::hasExternalChanges(juce::String& errorMessage) const
{
    errorMessage.clear();

    if (! isValid())
    {
        errorMessage = "The project session is not valid.";
        return false;
    }

    if (! containerFile.existsAsFile())
        return baselineRevision >= 0;

    ProjectManifest currentManifest;
    if (! ProjectContainerIO::readManifest(containerFile, currentManifest, errorMessage))
        return false;

    if (currentManifest.projectId != manifest.projectId)
    {
        errorMessage = "The target container path now points at a different project.";
        return true;
    }

    return baselineRevision >= 0 && currentManifest.revision != baselineRevision;
}

bool ProjectSession::reloadFromDisk(juce::String& errorMessage)
{
    if (! containerFile.existsAsFile())
    {
        errorMessage = "The suite project container no longer exists on disk.";
        return false;
    }

    ProjectSession reloaded;
    if (! open(containerFile, reloaded, errorMessage))
        return false;

    *this = std::move(reloaded);
    return true;
}

bool ProjectSession::commit(juce::String& errorMessage)
{
    if (! isValid())
    {
        errorMessage = "The project session is not valid.";
        return false;
    }

    juce::String externalStateError;
    const auto hasConflicts = hasExternalChanges(externalStateError);
    if (externalStateError.isNotEmpty() && ! hasConflicts)
    {
        errorMessage = externalStateError;
        return false;
    }

    if (hasConflicts)
    {
        errorMessage = externalStateError.isNotEmpty()
                           ? externalStateError
                           : "The suite project container changed on disk after this session was opened.";
        if (errorMessage == "The target container path now points at a different project.")
            return false;
        if (errorMessage.isEmpty())
            errorMessage = "The suite project container changed on disk after this session was opened.";
        else if (! errorMessage.containsIgnoreCase("different project"))
            errorMessage = "The suite project container changed on disk after this session was opened.";
        return false;
    }

    manifest.modifiedAt = juce::Time::getCurrentTime();
    if (! ProjectContainerIO::writeContainer(containerFile, manifest, entries, errorMessage))
        return false;

    baselineRevision = manifest.revision;
    return true;
}
}
