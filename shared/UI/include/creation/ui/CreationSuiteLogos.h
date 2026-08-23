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
    modeler,
    remote
};

std::array<SuiteLogoId, 9> getSuiteLogoIds();
juce::Image getSuiteLogoImage(SuiteLogoId logoId);
juce::String getSuiteLogoDisplayName(SuiteLogoId logoId);
juce::Colour getSuiteLogoAccentColour(SuiteLogoId logoId);

// The single, shared copyright line every app's startup splash/About box
// shows -- one place to spell this out, not retyped per app. "LagDaemon
// Software" is the COMPANY_NAME every app's CMakeLists.txt already uses
// consistently.
juce::String getSuiteCopyrightText();
}
