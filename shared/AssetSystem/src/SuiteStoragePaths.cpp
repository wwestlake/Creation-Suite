#include "creation/suite/SuiteStoragePaths.h"

namespace
{
juce::String fallbackIfEmpty(juce::String value, const juce::String& fallback)
{
    value = value.trim();
    return value.isNotEmpty() ? value : fallback;
}

juce::File getInternalSuiteDataDirectory()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("Creation Suite");
    return base;
}
}

namespace creation::suite
{
juce::String sanitizeProjectName(const juce::String& projectName)
{
    auto normalized = projectName.trim().toLowerCase();
    normalized = normalized.replaceCharacter('\\', '-')
                           .replaceCharacter('/', '-')
                           .replaceCharacter(':', '-')
                           .replaceCharacter(' ', '-');
    normalized = normalized.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_.");
    while (normalized.contains("--"))
        normalized = normalized.replace("--", "-");
    normalized = normalized.trimCharactersAtStart("-").trimCharactersAtEnd("-");
    return normalized.isNotEmpty() ? normalized : "untitled-project";
}

juce::String appDomainFolderName(creation::assets::SuiteAppDomain domain)
{
    switch (domain)
    {
        case creation::assets::SuiteAppDomain::station: return "Creation Station";
        case creation::assets::SuiteAppDomain::engine: return "Creation Engine";
        case creation::assets::SuiteAppDomain::engineer: return "Creation Engineer";
        case creation::assets::SuiteAppDomain::movie: return "Creation Movie";
        case creation::assets::SuiteAppDomain::live: return "Creation Live";
        case creation::assets::SuiteAppDomain::texture: return "Creation Texture";
        case creation::assets::SuiteAppDomain::modeler: return "Creation Modeler";
        case creation::assets::SuiteAppDomain::suite: return "Suite";
        case creation::assets::SuiteAppDomain::unknown: break;
    }

    return "Unknown";
}

juce::File getSuiteRootDirectory(const SuiteSettings& settings)
{
    auto suiteRoot = fallbackIfEmpty(settings.suiteVfsRoot,
                                     juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                         .getChildFile("Creation Suite")
                                         .getFullPathName());
    return juce::File(suiteRoot);
}

juce::File getSharedResourcesDirectory(const SuiteSettings& settings)
{
    return getSuiteRootDirectory(settings).getChildFile("Shared");
}

juce::File getContentDirectory(const SuiteSettings& settings)
{
    return getSuiteRootDirectory(settings).getChildFile("Content");
}

juce::File getTutorialsDirectory(const SuiteSettings& settings)
{
    return getSuiteRootDirectory(settings).getChildFile("Tutorials");
}

juce::File getTemplatesDirectory(const SuiteSettings& settings)
{
    return getSuiteRootDirectory(settings).getChildFile("Templates");
}

juce::File getCacheDirectory(const SuiteSettings& settings)
{
    return getSuiteRootDirectory(settings).getChildFile("Cache");
}

juce::File getAppProjectsDirectory(const SuiteSettings& settings,
                                   creation::assets::SuiteAppDomain domain)
{
    return getSuiteRootDirectory(settings)
        .getChildFile("Projects")
        .getChildFile(appDomainFolderName(domain));
}

juce::File getProjectContainerDirectory(const SuiteSettings& settings)
{
    return getSuiteRootDirectory(settings).getChildFile("Project Containers");
}

juce::File getProjectContainerPath(const SuiteSettings& settings,
                                   creation::assets::SuiteAppDomain domain,
                                   const juce::String& projectName)
{
    if (domain == creation::assets::SuiteAppDomain::suite)
        return getSuiteRootDirectory(settings).getChildFile("suite.csproj");

    auto baseDirectory = getProjectContainerDirectory(settings).getChildFile(appDomainFolderName(domain));
    return baseDirectory.getChildFile(sanitizeProjectName(projectName) + ".csproj");
}

juce::File getMaterializedFilesDirectory(const SuiteSettings& settings,
                                         const juce::String& projectId)
{
    auto baseDirectory = getCacheDirectory(settings).getChildFile("Materialized");
    return baseDirectory.getChildFile(sanitizeProjectName(projectId));
}

juce::File getExportDirectory(const SuiteSettings& settings,
                              creation::assets::SuiteAppDomain domain,
                              const juce::String& projectName)
{
    juce::ignoreUnused(settings);
    auto baseDirectory = getInternalSuiteDataDirectory().getChildFile("Exports");
    return baseDirectory.getChildFile(appDomainFolderName(domain))
                        .getChildFile(sanitizeProjectName(projectName));
}

creation::assets::ProjectManifest createDefaultManifest(const juce::String& projectName,
                                                        creation::assets::SuiteAppDomain domain,
                                                        const juce::String& suiteVersion,
                                                        const juce::String& appVersion)
{
    creation::assets::ProjectManifest manifest;
    manifest.projectId = juce::Uuid().toString();
    manifest.projectName = projectName.trim().isNotEmpty() ? projectName.trim() : "Untitled Project";
    manifest.appDomain = domain;
    manifest.schemaVersion = 1;
    manifest.revision = 0;
    manifest.createdWithSuiteVersion = suiteVersion;
    manifest.createdByAppVersion = appVersion;
    manifest.createdAt = juce::Time::getCurrentTime();
    manifest.modifiedAt = manifest.createdAt;
    return manifest;
}
}
