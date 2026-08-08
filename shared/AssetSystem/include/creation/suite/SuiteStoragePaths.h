#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectManifest.h"
#include "creation/suite/SuiteSettings.h"

namespace creation::suite
{
juce::String sanitizeProjectName(const juce::String& projectName);
juce::String appDomainFolderName(creation::assets::SuiteAppDomain domain);

juce::File getSuiteRootDirectory(const SuiteSettings& settings);
juce::File getSharedResourcesDirectory(const SuiteSettings& settings);
juce::File getContentDirectory(const SuiteSettings& settings);
juce::File getTutorialsDirectory(const SuiteSettings& settings);
juce::File getTemplatesDirectory(const SuiteSettings& settings);
juce::File getCacheDirectory(const SuiteSettings& settings);
juce::File getAppProjectsDirectory(const SuiteSettings& settings,
                                   creation::assets::SuiteAppDomain domain);
juce::File getProjectContainerDirectory(const SuiteSettings& settings);
juce::File getProjectContainerPath(const SuiteSettings& settings,
                                   creation::assets::SuiteAppDomain domain,
                                   const juce::String& projectName);
juce::File getMaterializedFilesDirectory(const SuiteSettings& settings,
                                         const juce::String& projectId);
juce::File getExportDirectory(const SuiteSettings& settings,
                              creation::assets::SuiteAppDomain domain,
                              const juce::String& projectName);

creation::assets::ProjectManifest createDefaultManifest(const juce::String& projectName,
                                                        creation::assets::SuiteAppDomain domain,
                                                        const juce::String& suiteVersion,
                                                        const juce::String& appVersion);
}
