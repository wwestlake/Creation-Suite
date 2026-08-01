#pragma once

#include <array>
#include <juce_gui_extra/juce_gui_extra.h>

namespace creation::ui
{
enum class SuiteLogoId
{
    suite,
    station,
    engine,
    movie,
    live,
    texture,
    modeler
};

std::array<SuiteLogoId, 7> getSuiteLogoIds();
juce::Image getSuiteLogoImage(SuiteLogoId logoId);
juce::String getSuiteLogoDisplayName(SuiteLogoId logoId);
juce::Colour getSuiteLogoAccentColour(SuiteLogoId logoId);
}
