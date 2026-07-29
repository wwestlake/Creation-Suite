#include "creation/assets/AssetMaterializer.h"

#include "creation/assets/VirtualFileSystem.h"
#include "creation/suite/SuiteStoragePaths.h"

namespace creation::assets
{
bool AssetMaterializer::materializeEntry(const creation::suite::SuiteSettings& settings,
                                         const juce::File& containerFile,
                                         const juce::String& projectId,
                                         const juce::String& logicalPath,
                                         MaterializedAssetLease& outLease,
                                         juce::String& errorMessage)
{
    VirtualFileSystem vfs;
    if (! vfs.mount(containerFile))
    {
        errorMessage = "Could not mount the suite project container for materialization.";
        return false;
    }

    const auto targetRoot = creation::suite::getMaterializedFilesDirectory(settings, projectId);
    if (! vfs.materializeEntry(logicalPath, targetRoot, errorMessage, true))
        return false;

    auto normalized = logicalPath.replaceCharacter('\\', '/');
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);

    outLease.logicalPath = normalized;
    outLease.projectId = projectId;
    outLease.materializedAt = juce::Time::getCurrentTime();
    outLease.materializedFile = targetRoot.getChildFile(normalized.replaceCharacter('/', juce::File::getSeparatorChar()));
    return outLease.materializedFile.existsAsFile();
}
}
