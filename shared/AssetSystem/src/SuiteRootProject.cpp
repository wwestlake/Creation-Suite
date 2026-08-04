#include "creation/assets/SuiteRootProject.h"

#include "creation/assets/ProjectContainerService.h"
#include "creation/suite/SuiteStoragePaths.h"

namespace creation::assets
{
bool SuiteRootProject::openOrCreate(const creation::suite::SuiteSettings& settings,
                                    ProjectSession& outSession,
                                    juce::String& errorMessage)
{
    const auto containerFile = creation::suite::getProjectContainerPath(settings, SuiteAppDomain::suite, kProjectName);
    if (containerFile.existsAsFile())
        return ProjectSession::open(containerFile, outSession, errorMessage);

    return ProjectContainerService::createProject(settings, SuiteAppDomain::suite, kProjectName, {}, {}, outSession, errorMessage);
}
}
