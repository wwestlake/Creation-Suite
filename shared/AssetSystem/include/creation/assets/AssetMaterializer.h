#pragma once

#include <cstdint>

#include <juce_core/juce_core.h>

#include "creation/suite/SuiteSettings.h"
#include "creation/vfs/SuiteVolume.h"

namespace creation::assets
{
enum class MaterializationAccess
{
    readOnly,
    writable
};

struct MaterializedAssetLease
{
    juce::File materializedFile;
    juce::File materializedRoot;
    juce::File leaseRoot;
    juce::String logicalPath;
    juce::String projectId;
    juce::String leaseId;
    juce::Time materializedAt;
    juce::Time sourceModifiedAt;
    std::int64_t sourceSizeBytes = 0;
    MaterializationAccess access = MaterializationAccess::readOnly;

    bool isValid() const noexcept { return materializedFile.existsAsFile(); }
};

// Reads an asset out of a live SuiteVolume mount and writes it to a real,
// isolated temp file (a "lease") for callers that need an actual file path
// (audio playback, external editors, etc). The volume passed in must already
// be open -- this never mounts its own second copy of a container, since two
// independent FatFs mounts of the same backing file would corrupt it.
class AssetMaterializer final
{
public:
    static bool materializeEntry(const creation::suite::SuiteSettings& settings,
                                 const creation::vfs::SuiteVolume& volume,
                                 const juce::String& projectId,
                                 const juce::String& logicalPath,
                                 MaterializationAccess access,
                                 MaterializedAssetLease& outLease,
                                 juce::String& errorMessage);

    static bool hasLocalChanges(const MaterializedAssetLease& lease);

    static bool releaseLease(MaterializedAssetLease& lease,
                             juce::String& errorMessage,
                             bool removeEmptyParentDirectories = true);
};
}
