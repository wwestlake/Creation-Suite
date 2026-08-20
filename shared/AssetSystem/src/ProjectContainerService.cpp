#include "creation/assets/ProjectContainerService.h"
#include "creation/services/SuiteVfsServiceClient.h"

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

    outSession = std::move(session);
    return true;
}

bool ProjectContainerService::openProject(const creation::suite::SuiteSettings& settings,
                                          const juce::String& projectId,
                                          ProjectSession& outSession,
                                          juce::String& errorMessage)
{
    return ProjectSession::open(settings, projectId, outSession, errorMessage);
}

juce::Array<ProjectContainerService::ProjectSummary> ProjectContainerService::listProjects(const creation::suite::SuiteSettings&,
                                                                                           SuiteAppDomain appDomain,
                                                                                           juce::String& errorMessage)
{
    juce::Array<ProjectSummary> results;
    errorMessage.clear();

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return results;
    }

    juce::Array<creation::services::SuiteVfsServiceClient::ProjectSummary> summaries;
    if (! client.listProjects(appDomain, summaries))
    {
        errorMessage = "Could not list projects for that domain.";
        return results;
    }

    for (const auto& summary : summaries)
    {
        ProjectSummary result;
        result.projectId = summary.projectId;
        result.manifest = summary.manifest;
        result.totalSizeBytes = summary.totalSizeBytes;
        results.add(std::move(result));
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
                               SuiteAppDomain::engineer,
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

bool ProjectContainerService::findProjectById(const creation::suite::SuiteSettings&,
                                              const juce::String& projectId,
                                              ProjectSummary& outProject,
                                              juce::String& errorMessage)
{
    errorMessage.clear();

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    ProjectManifest manifest;
    if (! client.readManifest(projectId, manifest))
    {
        errorMessage = "The requested suite project container was not found.";
        return false;
    }

    outProject.projectId = projectId;
    outProject.manifest = std::move(manifest);
    return true;
}

bool ProjectContainerService::cloneProject(const creation::suite::SuiteSettings&,
                                           const juce::String& sourceProjectId,
                                           const juce::String& newProjectName,
                                           juce::String& outNewProjectId,
                                           juce::String& errorMessage)
{
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    return client.cloneProject(sourceProjectId, newProjectName, outNewProjectId, errorMessage);
}

bool ProjectContainerService::deleteProject(const creation::suite::SuiteSettings&,
                                            const juce::String& projectId,
                                            juce::String& errorMessage)
{
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }

    return client.deleteProject(projectId, errorMessage);
}
}
