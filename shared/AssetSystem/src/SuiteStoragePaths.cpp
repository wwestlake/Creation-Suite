#include "creation/suite/SuiteStoragePaths.h"

namespace
{
juce::String fallbackIfEmpty(juce::String value, const juce::String& fallback)
{
    value = value.trim();
    return value.isNotEmpty() ? value : fallback;
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
        case creation::assets::SuiteAppDomain::movie: return "Creation Movie";
        case creation::assets::SuiteAppDomain::live: return "Creation Live";
        case creation::assets::SuiteAppDomain::texture: return "Creation Texture";
        case creation::assets::SuiteAppDomain::modeler: return "Creation Modeler";
        case creation::assets::SuiteAppDomain::unknown: break;
    }

    return "Unknown";
}

juce::File getAppProjectsDirectory(const SuiteSettings& settings,
                                   creation::assets::SuiteAppDomain domain)
{
    juce::String configured;

    switch (domain)
    {
        case creation::assets::SuiteAppDomain::station:
            configured = settings.creationStationProjectsRoot;
            break;
        case creation::assets::SuiteAppDomain::engine:
            configured = settings.creationEngineProjectsRoot;
            break;
        case creation::assets::SuiteAppDomain::movie:
            configured = settings.creationMovieProjectsRoot;
            break;
        case creation::assets::SuiteAppDomain::live:
            configured = settings.creationLiveProjectsRoot;
            break;
        case creation::assets::SuiteAppDomain::texture:
        case creation::assets::SuiteAppDomain::modeler:
        case creation::assets::SuiteAppDomain::unknown:
            break;
    }

    configured = configured.trim();
    if (configured.isNotEmpty())
        return juce::File(configured);

    auto suiteRoot = fallbackIfEmpty(settings.suiteVfsRoot, juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                            .getChildFile("Creation Suite")
                                                            .getFullPathName());
    return juce::File(suiteRoot)
        .getChildFile("Projects")
        .getChildFile(appDomainFolderName(domain));
}

juce::File getProjectContainerDirectory(const SuiteSettings& settings)
{
    auto configured = settings.projectContainersRoot.trim();
    if (configured.isNotEmpty())
        return juce::File(configured);

    auto suiteRoot = fallbackIfEmpty(settings.suiteVfsRoot, juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                            .getChildFile("Creation Suite")
                                                            .getFullPathName());
    return juce::File(suiteRoot).getChildFile("Project Containers");
}

juce::File getProjectContainerPath(const SuiteSettings& settings,
                                   creation::assets::SuiteAppDomain domain,
                                   const juce::String& projectName)
{
    auto baseDirectory = getProjectContainerDirectory(settings).getChildFile(appDomainFolderName(domain));
    return baseDirectory.getChildFile(sanitizeProjectName(projectName) + ".csproj");
}

juce::File getMaterializedFilesDirectory(const SuiteSettings& settings,
                                         const juce::String& projectId)
{
    auto configured = settings.materializedFilesRoot.trim();
    juce::File baseDirectory = configured.isNotEmpty()
                                 ? juce::File(configured)
                                 : getProjectContainerDirectory(settings).getSiblingFile("Cache").getChildFile("Materialized");
    return baseDirectory.getChildFile(sanitizeProjectName(projectId));
}

juce::File getExportDirectory(const SuiteSettings& settings,
                              creation::assets::SuiteAppDomain domain,
                              const juce::String& projectName)
{
    auto configured = settings.exportsRoot.trim();
    juce::File baseDirectory = configured.isNotEmpty()
                                 ? juce::File(configured)
                                 : getProjectContainerDirectory(settings).getSiblingFile("Exports");
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
