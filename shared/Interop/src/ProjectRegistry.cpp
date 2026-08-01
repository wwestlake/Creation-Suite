#include "creation/interop/ProjectRegistry.h"

#include "creation/assets/ProjectContainerService.h"

namespace
{
bool matchesQuery(const creation::interop::ProjectRecord& project, const creation::interop::ProjectQuery& query)
{
    if (query.appDomain.has_value() && project.manifest.appDomain != *query.appDomain)
        return false;

    if (query.searchText.isNotEmpty())
    {
        const auto haystack = project.manifest.projectName + "\n"
                            + project.manifest.projectId + "\n"
                            + creation::assets::toDisplayName(project.manifest.appDomain);
        if (! haystack.containsIgnoreCase(query.searchText))
            return false;
    }

    return true;
}
}

namespace creation::interop
{
juce::Array<ProjectRecord> ProjectRegistry::discoverProjects(const creation::suite::SuiteSettings& settings,
                                                             juce::String& errorMessage)
{
    juce::Array<ProjectRecord> projects;
    const auto summaries = creation::assets::ProjectContainerService::listAllProjects(settings, errorMessage);

    for (const auto& summary : summaries)
        projects.add({ summary.containerFile, summary.manifest });

    std::sort(projects.begin(), projects.end(), [](const ProjectRecord& left, const ProjectRecord& right)
    {
        return left.manifest.modifiedAt > right.manifest.modifiedAt;
    });

    return projects;
}

juce::Array<ProjectRecord> ProjectRegistry::queryProjects(const creation::suite::SuiteSettings& settings,
                                                          const ProjectQuery& query,
                                                          juce::String& errorMessage)
{
    auto allProjects = discoverProjects(settings, errorMessage);
    juce::Array<ProjectRecord> filtered;

    for (const auto& project : allProjects)
        if (matchesQuery(project, query))
            filtered.add(project);

    return filtered;
}
}
