#include "creation/suite/SuiteSettings.h"

namespace
{
juce::File getDefaultSuiteDataRoot()
{
    auto roamingAppData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto appDataRoot = roamingAppData.getParentDirectory();
    auto localAppData = appDataRoot.getChildFile("Local");
    if (localAppData.isDirectory())
        return localAppData.getChildFile("Creation Suite").getChildFile("Data");

    return roamingAppData.getChildFile("Creation Suite").getChildFile("Data");
}

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

void assignIfPresent(const juce::var& json, const juce::Identifier& propertyName, juce::String& target)
{
    auto value = readStringProperty(json, propertyName).trim();
    if (value.isNotEmpty())
        target = value;
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

    assignIfPresent(parsed, "suiteVfsRoot", settings.suiteVfsRoot);
    assignIfPresent(parsed, "sharedResourcesRoot", settings.sharedResourcesRoot);
    assignIfPresent(parsed, "projectContainersRoot", settings.projectContainersRoot);
    assignIfPresent(parsed, "cacheRoot", settings.cacheRoot);
    assignIfPresent(parsed, "materializedFilesRoot", settings.materializedFilesRoot);
    assignIfPresent(parsed, "exportsRoot", settings.exportsRoot);
    assignIfPresent(parsed, "creationStationProjectsRoot", settings.creationStationProjectsRoot);
    assignIfPresent(parsed, "creationEngineProjectsRoot", settings.creationEngineProjectsRoot);
    assignIfPresent(parsed, "creationMovieProjectsRoot", settings.creationMovieProjectsRoot);
    assignIfPresent(parsed, "creationLiveProjectsRoot", settings.creationLiveProjectsRoot);
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
    auto suiteRoot = getDefaultSuiteDataRoot();
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
