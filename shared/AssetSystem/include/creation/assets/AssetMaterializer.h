#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectContainerIO.h"
#include "creation/suite/SuiteSettings.h"

namespace creation::assets
{
struct MaterializedAssetLease
{
    juce::File materializedFile;
    juce::String logicalPath;
    juce::String projectId;
    juce::Time materializedAt;

    bool isValid() const noexcept { return materializedFile.existsAsFile(); }
};

class AssetMaterializer final
{
public:
    static bool materializeEntry(const creation::suite::SuiteSettings& settings,
                                 const juce::File& containerFile,
                                 const juce::String& projectId,
                                 const juce::String& logicalPath,
                                 MaterializedAssetLease& outLease,
                                 juce::String& errorMessage);
};
}
