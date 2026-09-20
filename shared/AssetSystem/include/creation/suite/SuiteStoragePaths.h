#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectManifest.h"
#include "creation/suite/SuiteSettings.h"

namespace creation::suite
{
juce::String sanitizeProjectName(const juce::String& projectName);
juce::String appDomainFolderName(creation::assets::SuiteAppDomain domain);

// True once the user has chosen a VFS root. Every directory below is INSIDE that root and is an empty juce::File until
// it is chosen: there is no fallback location (a fallback would put suite data on the system drive). Never call
// getChildFile()/createDirectory() on an empty result - check hasStorageRoot() first.
bool hasStorageRoot(const SuiteSettings& settings);

juce::File getSuiteRootDirectory(const SuiteSettings& settings);

// The one container file that holds the whole VFS (vfs.bin), directly inside the VFS root. Empty File when no root is
// chosen. Only the VFS service opens it.
juce::File getVfsContainerFile(const SuiteSettings& settings);
juce::File getSharedResourcesDirectory(const SuiteSettings& settings);
juce::File getContentDirectory(const SuiteSettings& settings);
juce::File getTutorialsDirectory(const SuiteSettings& settings);
juce::File getTemplatesDirectory(const SuiteSettings& settings);
juce::File getCacheDirectory(const SuiteSettings& settings);
// Temporary real files (a render in progress, a recording take, a decoded copy): inside the VFS root, never in the OS
// temp folder. See getCurrentScratchDirectory().
juce::File getScratchDirectory(const SuiteSettings& settings);
juce::File getLogsDirectory(const SuiteSettings& settings);
juce::File getProjectContainerDirectory(const SuiteSettings& settings);
juce::File getMaterializedFilesDirectory(const SuiteSettings& settings,
                                         const juce::String& projectId);
juce::File getExportDirectory(const SuiteSettings& settings,
                              creation::assets::SuiteAppDomain domain,
                              const juce::String& projectName);

// The scratch folder for the settings currently saved (loads them). Every temporary real file the suite needs goes
// here, inside the VFS root - nothing ever uses the OS temp folder. Empty File when no root is chosen: check it
// before writing.
juce::File getCurrentScratchDirectory();
creation::assets::ProjectManifest createDefaultManifest(const juce::String& projectName,
                                                        creation::assets::SuiteAppDomain domain,
                                                        const juce::String& suiteVersion,
                                                        const juce::String& appVersion);
}
