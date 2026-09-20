#include "creation/suite/SuiteStoragePaths.h"

namespace
{
// A directory inside the VFS root, or an empty File when no root is chosen (never a fallback location).
juce::File insideRoot(const creation::suite::SuiteSettings& settings, const char* relativePath)
{
    const auto root = settings.suiteVfsRoot.trim();
    if (root.isEmpty())
        return {};

    return juce::File(root).getChildFile(relativePath);
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
        case creation::assets::SuiteAppDomain::station: return "Djehuti Station";
        case creation::assets::SuiteAppDomain::engine: return "Djehuti Engine";
        case creation::assets::SuiteAppDomain::movie: return "Djehuti Movie";
        case creation::assets::SuiteAppDomain::live: return "Djehuti Live";
        case creation::assets::SuiteAppDomain::texture: return "Djehuti Texture";
        case creation::assets::SuiteAppDomain::modeler: return "Djehuti Modeler";
        case creation::assets::SuiteAppDomain::developer: return "Djehuti Developer";
        case creation::assets::SuiteAppDomain::engineer: return "Djehuti Engineer";
        case creation::assets::SuiteAppDomain::suite: return "Suite";
        case creation::assets::SuiteAppDomain::unknown: break;
    }

    return "Unknown";
}

bool hasStorageRoot(const SuiteSettings& settings)
{
    return settings.suiteVfsRoot.trim().isNotEmpty();
}

juce::File getSuiteRootDirectory(const SuiteSettings& settings)
{
    const auto root = settings.suiteVfsRoot.trim();
    return root.isNotEmpty() ? juce::File(root) : juce::File();
}

juce::File getSharedResourcesDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Shared");
}

juce::File getContentDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Content");
}

juce::File getTutorialsDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Tutorials");
}

juce::File getTemplatesDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Templates");
}

juce::File getCacheDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Cache");
}

juce::File getScratchDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Scratch");
}

juce::File getLogsDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Logs");
}

juce::File getProjectContainerDirectory(const SuiteSettings& settings)
{
    return insideRoot(settings, "Project Containers");
}

juce::File getMaterializedFilesDirectory(const SuiteSettings& settings,
                                         const juce::String& projectId)
{
    const auto cache = getCacheDirectory(settings);
    if (cache == juce::File())
        return {};

    return cache.getChildFile("Materialized").getChildFile(sanitizeProjectName(projectId));
}

juce::File getExportDirectory(const SuiteSettings& settings,
                              creation::assets::SuiteAppDomain domain,
                              const juce::String& projectName)
{
    const auto exportsRoot = insideRoot(settings, "Exports");
    if (exportsRoot == juce::File())
        return {};

    return exportsRoot.getChildFile(appDomainFolderName(domain))
                      .getChildFile(sanitizeProjectName(projectName));
}

juce::File getCurrentScratchDirectory()
{
    juce::String error;
    const auto scratch = getScratchDirectory(SuiteSettingsStore().load(error));
    if (scratch != juce::File() && ! scratch.exists())
        scratch.createDirectory();

    return scratch;
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
