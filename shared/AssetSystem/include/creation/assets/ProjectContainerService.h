#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectSession.h"

namespace creation::assets
{
class ProjectContainerService final
{
public:
    struct ProjectSummary
    {
        juce::String projectId;
        ProjectManifest manifest;
        std::int64_t totalSizeBytes = 0;
    };

    static bool createProject(const creation::suite::SuiteSettings& settings,
                              SuiteAppDomain appDomain,
                              const juce::String& projectName,
                              const juce::String& suiteVersion,
                              const juce::String& appVersion,
                              ProjectSession& outSession,
                              juce::String& errorMessage);

    static bool openProject(const creation::suite::SuiteSettings& settings,
                            const juce::String& projectId,
                            ProjectSession& outSession,
                            juce::String& errorMessage);

    static juce::Array<ProjectSummary> listProjects(const creation::suite::SuiteSettings& settings,
                                                    SuiteAppDomain appDomain,
                                                    juce::String& errorMessage);

    static juce::Array<ProjectSummary> listAllProjects(const creation::suite::SuiteSettings& settings,
                                                       juce::String& errorMessage);

    static bool findProjectById(const creation::suite::SuiteSettings& settings,
                                const juce::String& projectId,
                                ProjectSummary& outProject,
                                juce::String& errorMessage);

    static bool cloneProject(const creation::suite::SuiteSettings& settings,
                             const juce::String& sourceProjectId,
                             const juce::String& newProjectName,
                             juce::String& outNewProjectId,
                             juce::String& errorMessage);

    static bool deleteProject(const creation::suite::SuiteSettings& settings,
                              const juce::String& projectId,
                              juce::String& errorMessage);
};
}
