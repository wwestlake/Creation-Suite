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
        case SuiteLogoId::movie:
            return loadLogo(BinaryData::creation_movie_png, BinaryData::creation_movie_pngSize);
        case SuiteLogoId::live:
            return loadLogo(BinaryData::creation_live_png, BinaryData::creation_live_pngSize);
        case SuiteLogoId::texture:
            return loadLogo(BinaryData::creation_texture_png, BinaryData::creation_texture_pngSize);
        case SuiteLogoId::modeler:
            return loadLogo(BinaryData::creation_modeler_png, BinaryData::creation_modeler_pngSize);
    }

    return {};
}
}
