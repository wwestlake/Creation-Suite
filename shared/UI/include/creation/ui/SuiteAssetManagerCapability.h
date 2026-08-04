#pragma once

#include <juce_core/juce_core.h>

namespace creation::ui
{
// What the host app opening the Suite Asset Manager can actually do with a
// given file extension. Deliberately separate from creation::assets::AssetKind --
// this is per-app runtime capability (what THIS app's build can parse/run),
// not asset taxonomy.
struct SuiteAssetManagerCapability
{
    juce::String hostAppDisplayName;
    juce::StringArray parseableExtensions;
    juce::StringArray runnableExtensions;

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
