#include <creation/ui/CreationSuiteLogos.h>
#include <BinaryData.h>

namespace
{
juce::Image loadLogo(const char* data, int size)
{
    if (data == nullptr || size <= 0)
        return {};

    return juce::ImageCache::getFromMemory(data, size);
}
}

namespace creation::ui
{
std::array<SuiteLogoId, 9> getSuiteLogoIds()
{
    return { SuiteLogoId::suite,
             SuiteLogoId::texture,
             SuiteLogoId::modeler,
             SuiteLogoId::station,
             SuiteLogoId::engine,
             SuiteLogoId::engineer,
             SuiteLogoId::movie,
             SuiteLogoId::live,
             SuiteLogoId::remote };
}

juce::Image getSuiteLogoImage(SuiteLogoId logoId)
{
    switch (logoId)
    {
        case SuiteLogoId::suite:
            return loadLogo(BinaryData::creation_suite_png, BinaryData::creation_suite_pngSize);
        case SuiteLogoId::station:
            return loadLogo(BinaryData::creation_station_png, BinaryData::creation_station_pngSize);
        case SuiteLogoId::engine:
            return loadLogo(BinaryData::creation_engine_png, BinaryData::creation_engine_pngSize);
        case SuiteLogoId::engineer:
            return loadLogo(BinaryData::creation_engineer_png, BinaryData::creation_engineer_pngSize);
        case SuiteLogoId::movie:
            return loadLogo(BinaryData::creation_movie_png, BinaryData::creation_movie_pngSize);
        case SuiteLogoId::live:
            return loadLogo(BinaryData::creation_live_png, BinaryData::creation_live_pngSize);
        case SuiteLogoId::texture:
            return loadLogo(BinaryData::creation_texture_png, BinaryData::creation_texture_pngSize);
        case SuiteLogoId::modeler:
            return loadLogo(BinaryData::creation_modeler_png, BinaryData::creation_modeler_pngSize);
        case SuiteLogoId::remote:
            // No dedicated badge art yet -- reuses the generic suite mark as a
            // placeholder, same as Modeler/Movie/Texture's still-pending icon
            // refresh. Swap for real Creation Remote Receiver art when available.
            return loadLogo(BinaryData::creation_suite_png, BinaryData::creation_suite_pngSize);
    }

    return {};
}

juce::String getSuiteLogoDisplayName(SuiteLogoId logoId)
{
    switch (logoId)
    {
        case SuiteLogoId::suite: return "Creation Suite";
        case SuiteLogoId::station: return "Creation Station";
        case SuiteLogoId::engine: return "Creation Engine";
        case SuiteLogoId::engineer: return "Creation Engineer";
        case SuiteLogoId::movie: return "Creation Movie";
        case SuiteLogoId::live: return "Creation Live";
        case SuiteLogoId::texture: return "Creation Texture";
        case SuiteLogoId::modeler: return "Creation Modeler";
        case SuiteLogoId::remote: return "Creation Remote Receiver";
    }

    return "Creation Suite";
}

juce::Colour getSuiteLogoAccentColour(SuiteLogoId logoId)
{
    switch (logoId)
    {
        case SuiteLogoId::suite: return juce::Colour(0xffffc96a);
        case SuiteLogoId::station: return juce::Colour(0xffd16dff);
        case SuiteLogoId::engine: return juce::Colour(0xffff6a55);
        case SuiteLogoId::engineer: return juce::Colour(0xffff9b54);
        case SuiteLogoId::movie: return juce::Colour(0xff5f96ff);
        case SuiteLogoId::live: return juce::Colour(0xffd4ec59);
        case SuiteLogoId::texture: return juce::Colour(0xff68cfff);
        case SuiteLogoId::modeler: return juce::Colour(0xff4de0c7);
        case SuiteLogoId::remote: return juce::Colour(0xff9aa5b1);
    }

    return juce::Colours::white;
}

juce::String getSuiteCopyrightText()
{
    const auto year = juce::Time::getCurrentTime().getYear();
    return juce::String(juce::CharPointer_UTF8("\xc2\xa9")) + " " + juce::String(year)
         + " LagDaemon Software. All rights reserved.";
}
}
