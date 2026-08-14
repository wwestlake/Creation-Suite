#include "VfsProjectStore.h"

#include <creation/suite/SuiteStoragePaths.h>

using namespace creation::assets;
using namespace creation::suite;

namespace
{
// Projects are folders named by their id (a UUID), not their (possibly colliding, renameable)
// display name -- this makes find-by-id an O(1) path join instead of a scan, and sidesteps name
// collisions entirely. listProjects still enumerates + reads each manifest for display purposes.
juce::File domainFolder(const SuiteSettings& settings, SuiteAppDomain domain)
{
    return getProjectContainerDirectory(settings).getChildFile(appDomainFolderName(domain));
}

juce::File projectFolder(const SuiteSettings& settings, SuiteAppDomain domain, const juce::String& projectId)
{
    return domainFolder(settings, domain).getChildFile(projectId);
}
}

VfsProjectStore::VfsProjectStore(const SuiteSettings& settings) : settings_(settings) {}

juce::File VfsProjectStore::suiteRootFolder() const
{
    // Callers (Main.cpp) already prefix logical paths with "suite/" (normalizeEntryPath), so
    // this is the VFS root itself, not a "suite" subfolder -- the prefix in the path is what
    // creates the "suite/" subdirectory on disk, matching the layout the old container used.
    return getSuiteRootDirectory(settings_);
}

juce::String VfsProjectStore::normalizeLogicalPath(const juce::String& logicalPath)
{
    auto normalized = logicalPath.replaceCharacter('\\', '/').trim();
    while (normalized.startsWithChar('/'))
        normalized = normalized.substring(1);
    return normalized;
}

bool VfsProjectStore::readManifestFromFolder(const juce::File& projectFolder_, ProjectManifest& outManifest,
                                             juce::String& errorMessage)
{
    const auto manifestFile = projectFolder_.getChildFile(ProjectContainerPaths::manifestPath);
    if (! manifestFile.existsAsFile())
    {
        errorMessage = "The project folder does not contain a project manifest.";
        return false;
    }

    return deserializeManifest(manifestFile.loadFileAsString(), outManifest, errorMessage);
}

bool VfsProjectStore::writeManifestToFolder(const juce::File& projectFolder_, const ProjectManifest& manifest,
                                            juce::String& errorMessage)
{
    const auto manifestFile = projectFolder_.getChildFile(ProjectContainerPaths::manifestPath);
    if (! manifestFile.getParentDirectory().createDirectory())
    {
        errorMessage = "Could not create the project manifest directory.";
        return false;
    }

    const auto text = serializeManifest(manifest, true);
    if (! manifestFile.replaceWithText(text))
    {
        errorMessage = "Could not write the project manifest.";
        return false;
    }

    return true;
}

bool VfsProjectStore::readEntryFromFolder(const juce::File& rootFolder, const juce::String& logicalPath,
                                          juce::MemoryBlock& outData)
{
    const auto file = rootFolder.getChildFile(normalizeLogicalPath(logicalPath));
    if (! file.existsAsFile())
        return false;

    outData.reset();
    return file.loadFileAsData(outData);
}

bool VfsProjectStore::writeEntryToFolder(const juce::File& rootFolder, const juce::String& logicalPath,
                                         const juce::MemoryBlock& data, juce::String& errorMessage)
{
    const auto normalized = normalizeLogicalPath(logicalPath);
    if (normalized.isEmpty() || normalized == ProjectContainerPaths::manifestPath)
    {
        errorMessage = "Refusing to write to that logical path.";
        return false;
    }

    const auto file = rootFolder.getChildFile(normalized);
    if (! file.getParentDirectory().createDirectory())
    {
        errorMessage = "Could not create the entry's parent directory.";
        return false;
    }

    if (! file.replaceWithData(data.getData(), data.getSize()))
    {
        errorMessage = "Could not write the entry.";
        return false;
    }

    return true;
}

bool VfsProjectStore::removeEntryFromFolder(const juce::File& rootFolder, const juce::String& logicalPath)
{
    const auto file = rootFolder.getChildFile(normalizeLogicalPath(logicalPath));
    if (! file.existsAsFile())
        return false;

    return file.deleteFile();
}

juce::StringArray VfsProjectStore::listEntryPathsInFolder(const juce::File& rootFolder, const juce::String& excludePath)
{
    juce::StringArray paths;
    if (! rootFolder.isDirectory())
        return paths;

    juce::Array<juce::File> files;
    rootFolder.findChildFiles(files, juce::File::findFiles, true, "*");
    for (const auto& file : files)
    {
        auto relative = file.getRelativePathFrom(rootFolder).replaceCharacter('\\', '/');
        if (relative != excludePath)
            paths.add(relative);
    }

    return paths;
}

bool VfsProjectStore::createProject(SuiteAppDomain appDomain, const juce::String& projectName,
                                    const juce::String& suiteVersion, const juce::String& appVersion,
                                    juce::String& outProjectId, ProjectManifest& outManifest,
                                    juce::String& errorMessage)
{
    outManifest = createDefaultManifest(projectName, appDomain, suiteVersion, appVersion);
    outProjectId = outManifest.projectId;

    const auto folder = projectFolder(settings_, appDomain, outProjectId);
    if (! folder.createDirectory())
    {
        errorMessage = "Could not create the project directory.";
        return false;
    }

    return writeManifestToFolder(folder, outManifest, errorMessage);
}

bool VfsProjectStore::readManifest(const juce::String& projectId, ProjectManifest& outManifest,
                                   juce::String& errorMessage) const
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    return readManifestFromFolder(folder, outManifest, errorMessage);
}

bool VfsProjectStore::writeManifest(const juce::String& projectId, const ProjectManifest& manifest,
                                    juce::String& errorMessage)
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    return writeManifestToFolder(folder, manifest, errorMessage);
}

bool VfsProjectStore::listProjects(SuiteAppDomain appDomain, juce::Array<ProjectSummary>& outProjects) const
{
    outProjects.clear();
    const auto domainDir = domainFolder(settings_, appDomain);
    if (! domainDir.isDirectory())
        return true;

    juce::Array<juce::File> subdirectories;
    domainDir.findChildFiles(subdirectories, juce::File::findDirectories, false);

    for (const auto& folder : subdirectories)
    {
        ProjectSummary summary;
        juce::String errorMessage;
        if (! readManifestFromFolder(folder, summary.manifest, errorMessage))
            continue; // not a real project folder (or a corrupt one) -- skip it, don't fail the whole listing.

        summary.projectId = folder.getFileName();

        juce::Array<juce::File> allFiles;
        folder.findChildFiles(allFiles, juce::File::findFiles, true, "*");
        for (const auto& file : allFiles)
            summary.totalSizeBytes += file.getSize();

        outProjects.add(summary);
    }

    return true;
}

bool VfsProjectStore::findProjectFolderById(const juce::String& projectId, juce::File& outFolder) const
{
    if (projectId.isEmpty())
        return false;

    for (auto domain : { SuiteAppDomain::station, SuiteAppDomain::engine, SuiteAppDomain::movie,
                        SuiteAppDomain::live, SuiteAppDomain::texture, SuiteAppDomain::modeler })
    {
        auto folder = projectFolder(settings_, domain, projectId);
        if (folder.isDirectory())
        {
            outFolder = folder;
            return true;
        }
    }

    return false;
}

bool VfsProjectStore::cloneProject(const juce::String& sourceProjectId, const juce::String& newProjectName,
                                   juce::String& outNewProjectId, juce::String& errorMessage)
{
    juce::File sourceFolder;
    if (! findProjectFolderById(sourceProjectId, sourceFolder))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    ProjectManifest sourceManifest;
    if (! readManifestFromFolder(sourceFolder, sourceManifest, errorMessage))
        return false;

    ProjectManifest newManifest = sourceManifest;
    newManifest.projectId = juce::Uuid().toString();
    newManifest.projectName = newProjectName;
    newManifest.createdAt = juce::Time::getCurrentTime();
    newManifest.modifiedAt = newManifest.createdAt;
    outNewProjectId = newManifest.projectId;

    const auto destinationFolder = projectFolder(settings_, sourceManifest.appDomain, outNewProjectId);
    if (! destinationFolder.createDirectory())
    {
        errorMessage = "Could not create the cloned project directory.";
        return false;
    }

    juce::Array<juce::File> sourceFiles;
    sourceFolder.findChildFiles(sourceFiles, juce::File::findFiles, true, "*");
    for (const auto& sourceFile : sourceFiles)
    {
        const auto relative = sourceFile.getRelativePathFrom(sourceFolder);
        const auto destinationFile = destinationFolder.getChildFile(relative);
        if (! destinationFile.getParentDirectory().createDirectory() || ! sourceFile.copyFileTo(destinationFile))
        {
            errorMessage = "Could not copy \"" + relative + "\" into the cloned project.";
            return false;
        }
    }

    return writeManifestToFolder(destinationFolder, newManifest, errorMessage);
}

bool VfsProjectStore::deleteProject(const juce::String& projectId, juce::String& errorMessage)
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }

    if (! folder.deleteRecursively())
    {
        errorMessage = "Could not delete the project folder.";
        return false;
    }

    return true;
}

bool VfsProjectStore::readEntry(const juce::String& projectId, const juce::String& logicalPath,
                                juce::MemoryBlock& outData) const
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
        return false;
    return readEntryFromFolder(folder, logicalPath, outData);
}

bool VfsProjectStore::writeEntry(const juce::String& projectId, const juce::String& logicalPath,
                                 const juce::MemoryBlock& data, juce::String& errorMessage)
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
    {
        errorMessage = "No project with that id was found.";
        return false;
    }
    return writeEntryToFolder(folder, logicalPath, data, errorMessage);
}

bool VfsProjectStore::removeEntry(const juce::String& projectId, const juce::String& logicalPath)
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
        return false;
    return removeEntryFromFolder(folder, logicalPath);
}

juce::StringArray VfsProjectStore::listEntryPaths(const juce::String& projectId) const
{
    juce::File folder;
    if (! findProjectFolderById(projectId, folder))
        return {};
    return listEntryPathsInFolder(folder, ProjectContainerPaths::manifestPath);
}

bool VfsProjectStore::readSuiteEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    return readEntryFromFolder(suiteRootFolder(), logicalPath, outData);
}

bool VfsProjectStore::writeSuiteEntry(const juce::String& logicalPath, const juce::MemoryBlock& data)
{
    juce::String errorMessage;
    return writeEntryToFolder(suiteRootFolder(), logicalPath, data, errorMessage);
}

bool VfsProjectStore::removeSuiteEntry(const juce::String& logicalPath)
{
    return removeEntryFromFolder(suiteRootFolder(), logicalPath);
}

juce::StringArray VfsProjectStore::listSuiteEntryPaths() const
{
    return listEntryPathsInFolder(suiteRootFolder(), juce::String());
}
