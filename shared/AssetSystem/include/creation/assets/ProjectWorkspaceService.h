#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectAssetService.h"
#include "creation/assets/ProjectContainerService.h"

namespace creation::assets
{
class ProjectWorkspaceService final
{
public:
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

    static bool openProjectById(const creation::suite::SuiteSettings& settings,
                                const juce::String& projectId,
                                ProjectSession& outSession,
                                juce::String& errorMessage);

    static bool importFile(ProjectSession& session,
                           const juce::File& sourceFile,
                           const ProjectAssetService::ImportOptions& options,
                           AssetDescriptor& outDescriptor,
                           juce::String& errorMessage);

    static bool createNewAssetVersion(ProjectSession& session,
                                      const AssetDescriptor& existingAsset,
                                      const juce::File& sourceFile,
                                      const ProjectAssetService::ImportOptions& overrides,
                                      AssetDescriptor& outDescriptor,
                                      juce::String& errorMessage);

    static bool materializeAsset(ProjectSession& session,
                                 const creation::suite::SuiteSettings& settings,
                                 const AssetRef& reference,
                                 MaterializationAccess access,
                                 MaterializedAssetLease& outLease,
                                 juce::String& errorMessage);

    static bool reconcileAsset(ProjectSession& session,
                               const creation::suite::SuiteSettings& settings,
                               MaterializedAssetLease& lease,
                               juce::String& errorMessage,
                               bool releaseLeaseAfterCommit = true);
};
}
