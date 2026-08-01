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
        juce::File containerFile;
        ProjectManifest manifest;
    };

    static bool createProject(const creation::suite::SuiteSettings& settings,
                              SuiteAppDomain appDomain,
                              const juce::String& projectName,
                              const juce::String& suiteVersion,
                              const juce::String& appVersion,
                              ProjectSession& outSession,
                              juce::String& errorMessage);

    static bool openProject(const juce::File& containerFile,
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
};
}
