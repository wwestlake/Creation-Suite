#include "creation/suite/SuiteStoragePaths.h"
#include "creation/suite/SuiteSettings.h"

namespace
{
juce::File getDefaultSuiteDataRoot()
{
    auto roamingAppData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    auto appDataRoot = roamingAppData.getParentDirectory();
    auto localAppData = appDataRoot.getChildFile("Local");
    if (localAppData.isDirectory())
        return localAppData.getChildFile("Djehuti Suite").getChildFile("Data");

    return roamingAppData.getChildFile("Djehuti Suite").getChildFile("Data");
}

juce::var createJsonObject(const creation::suite::SuiteSettings& settings)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("suiteVfsRoot", settings.suiteVfsRoot);
    object->setProperty("suiteExecutablesRoot", settings.suiteExecutablesRoot);
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
    assignIfPresent(parsed, "suiteExecutablesRoot", settings.suiteExecutablesRoot);
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
        .getChildFile("Djehuti Suite");
}

juce::File SuiteSettingsStore::getSuiteSettingsFile() const
{
    return getSuiteConfigDirectory().getChildFile("suite-settings.json");
}

SuiteSettings SuiteSettingsStore::makeDefaultSettings() const
{
    auto suiteRoot = getDefaultSuiteDataRoot();

    SuiteSettings settings;
    settings.suiteVfsRoot = suiteRoot.getFullPathName();
    // Was hardcoded to "codex-{debug,release}-bin" regardless of which
    // agent's build was actually running -- every agent that never
    // explicitly overrides this in suite-settings.json silently tried to
    // launch/discover Codex's VFS service instead of its own, which is
    // exactly what caused Djehuti Station to hang indefinitely on startup:
    // whatever state Codex's own bin directory/service happened to be in
    // (stale, locked, mid-rebuild by Codex's own session) determined
    // whether every OTHER agent's app could even start.
    //
    // The build already copies each agent's own executables into its own
    // <agent>-{debug,release}-bin directory (see
    // creation_suite_get_shared_bin_dir/creation_suite_add_shared_bin_copy
    // in CreationSuiteBuildSettings.cmake) -- so the currently-running
    // executable's own directory is always the correct, per-agent default,
    // with no need to separately know or hardcode which agent this is.
    auto currentDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
    if (currentDir.getChildFile("DjehutiSuiteVfsService.exe").existsAsFile())
    {
        settings.suiteExecutablesRoot = currentDir.getFullPathName();
    }
    else
    {
        juce::String currentPath = currentDir.getFullPathName();
        juce::String agentPrefix = "gemini";
        if (currentPath.containsIgnoreCase("CreationSuite-Claude")) agentPrefix = "claude";
        else if (currentPath.containsIgnoreCase("CreationSuite-Codex")) agentPrefix = "codex";

#if JUCE_DEBUG
        settings.suiteExecutablesRoot = "D:/CreationSuite-Workspaces/" + agentPrefix + "-debug-bin";
#else
        settings.suiteExecutablesRoot = "D:/CreationSuite-Workspaces/" + agentPrefix + "-release-bin";
#endif
    }
    return settings;
}
}
