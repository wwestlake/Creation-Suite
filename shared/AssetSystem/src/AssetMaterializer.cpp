#include "creation/assets/AssetMaterializer.h"

#include "creation/assets/VirtualFileSystem.h"
#include "creation/suite/SuiteStoragePaths.h"

namespace creation::assets
{
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

bool AssetMaterializer::materializeEntry(const creation::suite::SuiteSettings& settings,
                                         const juce::File& containerFile,
                                         const juce::String& projectId,
                                         const juce::String& logicalPath,
                                         MaterializationAccess access,
                                         MaterializedAssetLease& outLease,
                                         juce::String& errorMessage)
{
    VirtualFileSystem vfs;
    if (! vfs.mount(containerFile))
    {
        errorMessage = "Could not mount the suite project container for materialization.";
        return false;
    }

    const auto materializedRoot = creation::suite::getMaterializedFilesDirectory(settings, projectId);
    if (! materializedRoot.exists() && ! materializedRoot.createDirectory())
    {
        errorMessage = "Could not create the suite materialization root.";
        return false;
    }

    auto normalized = normalizeLogicalPath(logicalPath);
    auto leaseId = juce::Uuid().toString();
    auto leaseRoot = materializedRoot.getChildFile("leases").getChildFile(leaseId);

    if (! leaseRoot.createDirectory())
    {
        errorMessage = "Could not create the isolated materialization lease directory.";
        return false;
    }

    if (! vfs.materializeEntry(normalized, leaseRoot, errorMessage, true))
        return false;

    outLease.logicalPath = normalized;
    outLease.projectId = projectId;
    outLease.leaseId = leaseId;
    outLease.materializedAt = juce::Time::getCurrentTime();
    outLease.materializedRoot = materializedRoot;
    outLease.leaseRoot = leaseRoot;
    outLease.materializedFile = leaseRoot.getChildFile(normalized.replaceCharacter('/', juce::File::getSeparatorChar()));
    outLease.access = access;

    if (! outLease.materializedFile.existsAsFile())
    {
        errorMessage = "The materialized lease file was not created.";
        return false;
    }

    outLease.sourceModifiedAt = outLease.materializedFile.getLastModificationTime();
    outLease.sourceSizeBytes = outLease.materializedFile.getSize();

    if (access == MaterializationAccess::readOnly)
        outLease.materializedFile.setReadOnly(true);
    else
        outLease.materializedFile.setReadOnly(false);

    return true;
}

bool AssetMaterializer::hasLocalChanges(const MaterializedAssetLease& lease)
{
    if (! lease.isValid())
        return false;

    return lease.materializedFile.getSize() != lease.sourceSizeBytes
        || lease.materializedFile.getLastModificationTime() != lease.sourceModifiedAt;
}

bool AssetMaterializer::releaseLease(MaterializedAssetLease& lease,
                                     juce::String& errorMessage,
                                     bool removeEmptyParentDirectories)
{
    if (! lease.isValid())
    {
        lease = {};
        return true;
    }

    lease.materializedFile.setReadOnly(false);

    if (lease.leaseRoot != juce::File())
    {
        if (! lease.leaseRoot.deleteRecursively())
        {
            errorMessage = "Could not remove the isolated materialization lease directory.";
            return false;
        }
    }
    else if (! lease.materializedFile.deleteFile())
    {
        errorMessage = "Could not remove the materialized asset lease file.";
        return false;
    }

    if (removeEmptyParentDirectories)
    {
        auto parent = lease.leaseRoot != juce::File() ? lease.leaseRoot.getParentDirectory()
                                                      : lease.materializedFile.getParentDirectory();
        while (parent != juce::File() && parent != lease.materializedRoot)
        {
            if (! parent.isDirectory() || parent.getNumberOfChildFiles(juce::File::findFilesAndDirectories) > 0)
                break;

            if (! parent.deleteRecursively())
                break;

            parent = parent.getParentDirectory();
        }
    }

    lease = {};
    return true;
}
}
