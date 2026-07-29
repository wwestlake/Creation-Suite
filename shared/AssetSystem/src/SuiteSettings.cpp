#include "creation/suite/SuiteSettings.h"

namespace
{
juce::var createJsonObject(const creation::suite::SuiteSettings& settings)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("suiteVfsRoot", settings.suiteVfsRoot);
    object->setProperty("sharedResourcesRoot", settings.sharedResourcesRoot);
    object->setProperty("projectContainersRoot", settings.projectContainersRoot);
    object->setProperty("cacheRoot", settings.cacheRoot);
    object->setProperty("materializedFilesRoot", settings.materializedFilesRoot);
    object->setProperty("exportsRoot", settings.exportsRoot);
    object->setProperty("creationStationProjectsRoot", settings.creationStationProjectsRoot);
    object->setProperty("creationEngineProjectsRoot", settings.creationEngineProjectsRoot);
    object->setProperty("creationMovieProjectsRoot", settings.creationMovieProjectsRoot);
    object->setProperty("creationLiveProjectsRoot", settings.creationLiveProjectsRoot);
    return juce::var(object);
}

juce::String readStringProperty(const juce::var& json, const juce::Identifier& propertyName)
{
    if (auto* object = json.getDynamicObject())
        return object->getProperty(propertyName).toString();

    return {};
}
}

namespace creation::suite
{
SuiteSettingsStore::SuiteSettingsStore() = default;

SuiteSettings SuiteSettingsStore::load(juce::String& errorMessage) const
{
    auto settings = makeDefaultSettings();
    auto settingsFile = getSuiteSettingsFile();
    if (! settingsFile.existsAsFile())
        return settings;

    auto parsed = juce::JSON::parse(settingsFile);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the suite settings file. Using defaults.";
        return settings;
    }

    settings.suiteVfsRoot = readStringProperty(parsed, "suiteVfsRoot");
    settings.sharedResourcesRoot = readStringProperty(parsed, "sharedResourcesRoot");
    settings.projectContainersRoot = readStringProperty(parsed, "projectContainersRoot");
    settings.cacheRoot = readStringProperty(parsed, "cacheRoot");
    settings.materializedFilesRoot = readStringProperty(parsed, "materializedFilesRoot");
    settings.exportsRoot = readStringProperty(parsed, "exportsRoot");
    settings.creationStationProjectsRoot = readStringProperty(parsed, "creationStationProjectsRoot");
    settings.creationEngineProjectsRoot = readStringProperty(parsed, "creationEngineProjectsRoot");
    settings.creationMovieProjectsRoot = readStringProperty(parsed, "creationMovieProjectsRoot");
    settings.creationLiveProjectsRoot = readStringProperty(parsed, "creationLiveProjectsRoot");
    return settings;
}

bool SuiteSettingsStore::save(const SuiteSettings& settings, juce::String& errorMessage) const
{
    auto configDirectory = getSuiteConfigDirectory();
    if (! configDirectory.exists() && ! configDirectory.createDirectory())
    {
        errorMessage = "Could not create the suite configuration folder.";
        return false;
    }

    auto jsonText = juce::JSON::toString(createJsonObject(settings), true);
    if (! getSuiteSettingsFile().replaceWithText(jsonText))
    {
        errorMessage = "Could not save the suite settings file.";
        return false;
    }

    return true;
}

juce::File SuiteSettingsStore::getSuiteConfigDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Creation Suite");
}

juce::File SuiteSettingsStore::getSuiteSettingsFile() const
{
    return getSuiteConfigDirectory().getChildFile("suite-settings.json");
}

SuiteSettings SuiteSettingsStore::makeDefaultSettings() const
{
    auto suiteRoot = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Creation Suite");
    auto projectsRoot = suiteRoot.getChildFile("Projects");
    auto containersRoot = suiteRoot.getChildFile("Project Containers");
    auto cacheRoot = suiteRoot.getChildFile("Cache");
    auto materializedFilesRoot = cacheRoot.getChildFile("Materialized");
    auto exportsRoot = suiteRoot.getChildFile("Exports");

    SuiteSettings settings;
    settings.suiteVfsRoot = suiteRoot.getFullPathName();
    settings.sharedResourcesRoot = suiteRoot.getChildFile("Shared").getFullPathName();
    settings.projectContainersRoot = containersRoot.getFullPathName();
    settings.cacheRoot = cacheRoot.getFullPathName();
    settings.materializedFilesRoot = materializedFilesRoot.getFullPathName();
    settings.exportsRoot = exportsRoot.getFullPathName();
    settings.creationStationProjectsRoot = projectsRoot.getChildFile("Creation Station").getFullPathName();
    settings.creationEngineProjectsRoot = projectsRoot.getChildFile("Creation Engine").getFullPathName();
    settings.creationMovieProjectsRoot = projectsRoot.getChildFile("Creation Movie").getFullPathName();
    settings.creationLiveProjectsRoot = projectsRoot.getChildFile("Creation Live").getFullPathName();
    return settings;
}
}
