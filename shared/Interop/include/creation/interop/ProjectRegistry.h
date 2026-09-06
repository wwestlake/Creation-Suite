#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectManifest.h"
#include "creation/suite/SuiteSettings.h"

namespace creation::interop
{
struct ProjectRecord
{
    juce::String projectId;
    creation::assets::ProjectManifest manifest;
};

// Deliberately no appDomain field -- projects are not owned by any app, so app-domain is never
// a valid filter axis. See docs/architecture/Suite-Shared-Project-Model.md.
struct ProjectQuery
{
    juce::String searchText;
};

class ProjectRegistry final
{
public:
    static juce::Array<ProjectRecord> discoverProjects(const creation::suite::SuiteSettings& settings,
                                                       juce::String& errorMessage);
    static juce::Array<ProjectRecord> queryProjects(const creation::suite::SuiteSettings& settings,
                                                    const ProjectQuery& query,
                                                    juce::String& errorMessage);
};
}
