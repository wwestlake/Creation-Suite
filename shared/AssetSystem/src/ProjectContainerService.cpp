#include "creation/assets/ProjectContainerService.h"

namespace
{
juce::Array<juce::File> findContainerFiles(const juce::File& domainDirectory)
{
    juce::Array<juce::File> files;
    if (domainDirectory.isDirectory())
        domainDirectory.findChildFiles(files, juce::File::findFiles, false, "*.csproj");
    return files;
}
}

namespace creation::assets
{
bool ProjectContainerService::createProject(const creation::suite::SuiteSettings& settings,
                                            SuiteAppDomain appDomain,
                                            const juce::String& projectName,
                                            const juce::String& suiteVersion,
                                            const juce::String& appVersion,
                                            ProjectSession& outSession,
                                            juce::String& errorMessage)
{
    ProjectSession session;
    if (! ProjectSession::createNew(settings, appDomain, projectName, suiteVersion, appVersion, session, errorMessage))
        return false;

    if (! session.commit(errorMessage))
        return false;

    outSession = std::move(session);
    return true;
}

bool ProjectContainerService::openProject(const juce::File& containerFile,
                                          ProjectSession& outSession,
                                          juce::String& errorMessage)
{
    return ProjectSession::open(containerFile, outSession, errorMessage);
}

juce::Array<ProjectContainerService::ProjectSummary> ProjectContainerService::listProjects(const creation::suite::SuiteSettings& settings,
                                                                                           SuiteAppDomain appDomain,
                                                                                           juce::String& errorMessage)
{
    juce::Array<ProjectSummary> results;
    errorMessage.clear();

    const auto domainDirectory = creation::suite::getProjectContainerDirectory(settings)
                                     .getChildFile(creation::suite::appDomainFolderName(appDomain));

    for (const auto& containerFile : findContainerFiles(domainDirectory))
    {
        ProjectManifest manifest;
        juce::String manifestError;
        if (! ProjectSession::peekManifest(containerFile, manifest, manifestError))
        {
            if (errorMessage.isEmpty())
                errorMessage = "Some project containers could not be read.";
            continue;
        }

        ProjectSummary summary;
        summary.containerFile = containerFile;
        summary.manifest = std::move(manifest);
        results.add(std::move(summary));
    }

    std::sort(results.begin(), results.end(), [](const ProjectSummary& left, const ProjectSummary& right)
    {
        if (left.manifest.modifiedAt == right.manifest.modifiedAt)
            return left.manifest.projectName.compareIgnoreCase(right.manifest.projectName) < 0;
        return left.manifest.modifiedAt > right.manifest.modifiedAt;
    });

    return results;
}

juce::Array<ProjectContainerService::ProjectSummary> ProjectContainerService::listAllProjects(const creation::suite::SuiteSettings& settings,
                                                                                              juce::String& errorMessage)
{
    juce::Array<ProjectSummary> results;
    errorMessage.clear();

    for (const auto domain : { SuiteAppDomain::station,
                               SuiteAppDomain::engine,
                               SuiteAppDomain::movie,
                               SuiteAppDomain::live,
                               SuiteAppDomain::texture,
                               SuiteAppDomain::modeler })
    {
        juce::String listError;
        const auto domainProjects = listProjects(settings, domain, listError);
        results.addArray(domainProjects);

        if (errorMessage.isEmpty() && listError.isNotEmpty())
            errorMessage = listError;
    }

    std::sort(results.begin(), results.end(), [](const ProjectSummary& left, const ProjectSummary& right)
    {
        if (left.manifest.modifiedAt == right.manifest.modifiedAt)
            return left.manifest.projectName.compareIgnoreCase(right.manifest.projectName) < 0;
        return left.manifest.modifiedAt > right.manifest.modifiedAt;
    });

    return results;
}

bool ProjectContainerService::findProjectById(const creation::suite::SuiteSettings& settings,
                                              const juce::String& projectId,
                                              ProjectSummary& outProject,
                                              juce::String& errorMessage)
{
    errorMessage.clear();

    for (const auto domain : { SuiteAppDomain::station,
                               SuiteAppDomain::engine,
                               SuiteAppDomain::movie,
                               SuiteAppDomain::live,
                               SuiteAppDomain::texture,
                               SuiteAppDomain::modeler })
    {
        juce::String listError;
        const auto projects = listProjects(settings, domain, listError);
        for (const auto& project : projects)
        {
            if (project.manifest.projectId == projectId)
            {
                outProject = project;
                return true;
            }
        }

        if (errorMessage.isEmpty() && listError.isNotEmpty())
            errorMessage = listError;
    }

    if (errorMessage.isEmpty())
        errorMessage = "The requested suite project container was not found.";
    return false;
}
}
