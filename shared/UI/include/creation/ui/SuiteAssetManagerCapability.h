#pragma once

#include <creation/assets/ProjectManifest.h>
#include <juce_core/juce_core.h>
#include <functional>

namespace creation::ui
{
// What the host app opening the Suite Asset Manager can actually do with a
// given file extension. Deliberately separate from creation::assets::AssetKind --
// this is per-app runtime capability (what THIS app's build can parse/run),
// not asset taxonomy.
struct SuiteAssetManagerCapability
{
    juce::String hostAppDisplayName;
    creation::assets::SuiteAppDomain appDomain = creation::assets::SuiteAppDomain::unknown;
    juce::StringArray parseableExtensions;
    juce::StringArray runnableExtensions;
    std::function<juce::Array<creation::assets::AssetDescriptor>()> enumerateProjectAssets;
    std::function<juce::String(const creation::assets::AssetDescriptor&)> describeProjectAsset;
    std::function<void(const creation::assets::AssetDescriptor&)> openProjectAsset;
    std::function<void(const creation::assets::AssetDescriptor&)> stopProjectAsset;
    std::function<void(const creation::assets::AssetDescriptor&)> placeProjectAsset;
    std::function<void(const creation::assets::AssetDescriptor&)> exportProjectAsset;

    bool canParse(const juce::String& extension) const
    {
        return parseableExtensions.contains(extension.toLowerCase());
    }

    bool canRun(const juce::String& extension) const
    {
        return runnableExtensions.contains(extension.toLowerCase());
    }
};
}
