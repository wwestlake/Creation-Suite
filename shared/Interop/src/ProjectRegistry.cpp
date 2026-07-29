#include "creation/interop/ProjectRegistry.h"

#include "creation/assets/ProjectContainerIO.h"
#include "creation/suite/SuiteStoragePaths.h"

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

void scanDomainDirectory(const juce::File& domainDirectory,
                         juce::Array<creation::interop::ProjectRecord>& results)
{
    if (! domainDirectory.isDirectory())
        return;

    juce::Array<juce::File> children;
    domainDirectory.findChildFiles(children, juce::File::findFiles, false, "*.csproj");

    for (const auto& file : children)
    {
        creation::assets::ProjectManifest manifest;
        juce::String errorMessage;
        if (! creation::assets::ProjectContainerIO::readManifest(file, manifest, errorMessage))
            continue;

        results.add({ file, std::move(manifest) });
    }
}
}

namespace creation::interop
{
juce::Array<ProjectRecord> ProjectRegistry::discoverProjects(const creation::suite::SuiteSettings& settings,
                                                             juce::String& errorMessage)
{
    juce::ignoreUnused(errorMessage);

    juce::Array<ProjectRecord> projects;
    const auto root = creation::suite::getProjectContainerDirectory(settings);

    scanDomainDirectory(root.getChildFile("Creation Station"), projects);
    scanDomainDirectory(root.getChildFile("Creation Engine"), projects);
    scanDomainDirectory(root.getChildFile("Creation Movie"), projects);
    scanDomainDirectory(root.getChildFile("Creation Live"), projects);

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
