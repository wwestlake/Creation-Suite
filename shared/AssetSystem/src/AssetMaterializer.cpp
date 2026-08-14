#include "creation/assets/AssetMaterializer.h"

#include "creation/services/SuiteVfsServiceClient.h"
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
                                         const juce::String& projectId,
                                         const juce::String& logicalPath,
                                         MaterializationAccess access,
                                         MaterializedAssetLease& outLease,
                                         juce::String& errorMessage)
{
    const auto normalized = normalizeLogicalPath(logicalPath);

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    juce::MemoryBlock data;
    if (! client.readProjectEntry(projectId, normalized, data))
    {
        errorMessage = "Could not read the entry from the suite VFS service.";
        return false;
    }

    const auto materializedRoot = creation::suite::getMaterializedFilesDirectory(settings, projectId);
    if (! materializedRoot.exists() && ! materializedRoot.createDirectory())
    {
        errorMessage = "Could not create the suite materialization root.";
        return false;
    }

    const auto leaseId = juce::Uuid().toString();
    const auto leaseRoot = materializedRoot.getChildFile("leases").getChildFile(leaseId);
    if (! leaseRoot.createDirectory())
    {
        errorMessage = "Could not create the isolated materialization lease directory.";
        return false;
    }

    const auto materializedFile = leaseRoot.getChildFile(normalized.replaceCharacter('/', juce::File::getSeparatorChar()));
    if (! materializedFile.getParentDirectory().createDirectory())
    {
        errorMessage = "Could not create the materialization lease's asset directory.";
        return false;
    }

    if (! materializedFile.replaceWithData(data.getData(), data.getSize()))
    {
        errorMessage = "Could not write the materialized asset file.";
        return false;
    }

    outLease.logicalPath = normalized;
    outLease.projectId = projectId;
    outLease.leaseId = leaseId;
    outLease.materializedAt = juce::Time::getCurrentTime();
    outLease.materializedRoot = materializedRoot;
    outLease.leaseRoot = leaseRoot;
    outLease.materializedFile = materializedFile;
    outLease.access = access;
    outLease.sourceModifiedAt = materializedFile.getLastModificationTime();
    outLease.sourceSizeBytes = materializedFile.getSize();

    materializedFile.setReadOnly(access == MaterializationAccess::readOnly);

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
