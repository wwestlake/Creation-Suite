#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/AssetTypes.h"

namespace creation::assets
{
struct AssetCatalog
{
    juce::Array<AssetDescriptor> assets;

    const AssetDescriptor* findById(const AssetId& assetId) const noexcept;
    const AssetDescriptor* findByVersionId(const AssetVersionId& versionId) const noexcept;
    juce::Array<AssetDescriptor> findAllVersions(const AssetId& assetId) const;
    juce::Array<AssetDescriptor> query(const AssetQuery& query) const;
};

juce::var toVar(const AssetDescriptor& descriptor);
bool fromVar(const juce::var& value, AssetDescriptor& outDescriptor);

juce::var toVar(const AssetCatalog& catalog);
bool fromVar(const juce::var& value, AssetCatalog& outCatalog);
}
