#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

#include <creation/ui/SuiteAssetManagerCapability.h>

namespace creation::ui
{
class SuiteAssetManagerPanel final : public juce::Component
{
public:
    explicit SuiteAssetManagerPanel(SuiteAssetManagerCapability capability);
    ~SuiteAssetManagerPanel() override;

    void resized() override;

private:
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SuiteAssetManagerPanel)
};
}
