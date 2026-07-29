#include <creation/services/SuiteLegalSettings.h>
#include <creation/suite/SuiteSettings.h>

namespace
{
juce::File getSuiteConfigDirectory()
{
    return creation::suite::SuiteSettingsStore().getSuiteConfigDirectory();
}

juce::var toVar(const creation::services::SuiteLegalSettings& settings)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("eulaAccepted", settings.eulaAccepted);
    object->setProperty("acceptedEulaVersion", settings.acceptedEulaVersion);
    object->setProperty("acceptedAtMs", settings.acceptedAt.toMilliseconds());
    return juce::var(object);
}

creation::services::SuiteLegalSettings fromVar(const juce::var& value)
{
    creation::services::SuiteLegalSettings settings;
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return settings;

    settings.eulaAccepted = static_cast<bool>(object->getProperty("eulaAccepted"));
    settings.acceptedEulaVersion = object->getProperty("acceptedEulaVersion").toString();
    settings.acceptedAt = juce::Time(static_cast<int64_t>(object->getProperty("acceptedAtMs")));
    return settings;
}
}

namespace creation::services
{
SuiteLegalSettings SuiteLegalSettingsStore::load(juce::String& errorMessage) const
{
    auto settingsFile = getSettingsFile();
    if (! settingsFile.existsAsFile())
        return {};

    const auto parsed = juce::JSON::parse(settingsFile);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the suite legal settings file.";
        return {};
    }

    return fromVar(parsed);
}

bool SuiteLegalSettingsStore::save(const SuiteLegalSettings& settings, juce::String& errorMessage) const
{
    auto configDirectory = getSuiteConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the suite configuration folder for legal settings.";
        return false;
    }

    if (! getSettingsFile().replaceWithText(juce::JSON::toString(toVar(settings), true)))
    {
        errorMessage = "Could not save the suite legal settings file.";
        return false;
    }

    return true;
}

juce::File SuiteLegalSettingsStore::getSettingsFile() const
{
    return getSuiteConfigDirectory().getChildFile("suite-legal-settings.json");
}
}
