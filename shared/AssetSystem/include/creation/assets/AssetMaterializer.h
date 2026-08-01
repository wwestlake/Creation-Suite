#pragma once

#include <cstdint>

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectContainerIO.h"
#include "creation/suite/SuiteSettings.h"

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

class AssetMaterializer final
{
public:
    static bool materializeEntry(const creation::suite::SuiteSettings& settings,
                                 const juce::File& containerFile,
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
