#pragma once

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

namespace creation::ui
{
// Standardized "Save/Load" entry point for tools that let the user save their current work as a
// named project asset and reload it later (Tracker arrangements, Foley setups, Signal Lab
// patches, etc). Every tool gets the same button label and menu shape instead of inventing its
// own -- extracted from three near-identical copies of this exact PopupMenu+AlertWindow sequence
// (Tracker's old showArrangementMenu, Foley's showSetupMenu, and SuiteShellController's project
// rename/create flows).
//
// Shows a "Save.../Load..." popup menu anchored to `anchor`. Picking Save opens a name-entry
// dialog (pre-filled with `defaultName`); confirming calls `onSave(name)`. Picking Load calls
// `onLoad()` directly -- the caller owns building its own kind-filtered asset picker, matching
// the existing pattern where each tool's MainComponent builds its own Load popup listing only
// that tool's own asset kind.
void showNamedAssetSaveLoadMenu(juce::Component& anchor,
                                const juce::String& itemLabel,
                                const juce::String& defaultName,
                                std::function<void(const juce::String& name)> onSave,
                                std::function<void()> onLoad);
}
