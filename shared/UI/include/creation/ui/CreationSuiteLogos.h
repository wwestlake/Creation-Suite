#pragma once

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

juce::Image getSuiteLogoImage(SuiteLogoId logoId);
}
