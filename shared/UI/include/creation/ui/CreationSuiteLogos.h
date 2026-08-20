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
    engineer,
    movie,
    live,
    texture,
    modeler
};

std::array<SuiteLogoId, 8> getSuiteLogoIds();
juce::Image getSuiteLogoImage(SuiteLogoId logoId);
juce::String getSuiteLogoDisplayName(SuiteLogoId logoId);
juce::Colour getSuiteLogoAccentColour(SuiteLogoId logoId);
}
