#include "creation/assets/ProjectWorkspaceService.h"

namespace creation::assets
{
bool ProjectWorkspaceService::createProject(const creation::suite::SuiteSettings& settings,
                                            SuiteAppDomain appDomain,
                                            const juce::String& projectName,
                                            const juce::String& suiteVersion,
                                            const juce::String& appVersion,
                                            ProjectSession& outSession,
                                            juce::String& errorMessage)
{
    return ProjectContainerService::createProject(settings,
                                                  appDomain,
                                                  projectName,
                                                  suiteVersion,
                                                  appVersion,
                                                  outSession,
                                                  errorMessage);
}

bool ProjectWorkspaceService::openProject(const juce::File& containerFile,
                                          ProjectSession& outSession,
                                          juce::String& errorMessage)
{
    return ProjectContainerService::openProject(containerFile, outSession, errorMessage);
}

bool ProjectWorkspaceService::openProjectById(const creation::suite::SuiteSettings& settings,
                                              const juce::String& projectId,
                                              ProjectSession& outSession,
                                              juce::String& errorMessage)
{
    ProjectContainerService::ProjectSummary summary;
    if (! ProjectContainerService::findProjectById(settings, projectId, summary, errorMessage))
        return false;

    return openProject(summary.containerFile, outSession, errorMessage);
}

bool ProjectWorkspaceService::importFile(ProjectSession& session,
                                         const juce::File& sourceFile,
                                         const ProjectAssetService::ImportOptions& options,
                                         AssetDescriptor& outDescriptor,
                                         juce::String& errorMessage)
{
    if (! ProjectAssetService::importFile(session, sourceFile, options, outDescriptor, errorMessage))
        return false;

    return session.commit(errorMessage);
}

bool ProjectWorkspaceService::createNewAssetVersion(ProjectSession& session,
                                                    const AssetDescriptor& existingAsset,
                                                    const juce::File& sourceFile,
                                                    const ProjectAssetService::ImportOptions& overrides,
                                                    AssetDescriptor& outDescriptor,
                                                    juce::String& errorMessage)
{
    if (! ProjectAssetService::createNewVersion(session, existingAsset, sourceFile, overrides, outDescriptor, errorMessage))
        return false;

    return session.commit(errorMessage);
}

bool ProjectWorkspaceService::materializeAsset(ProjectSession& session,
                                               const creation::suite::SuiteSettings& settings,
                                               const AssetRef& reference,
                                               MaterializationAccess access,
                                               MaterializedAssetLease& outLease,
                                               juce::String& errorMessage)
{
    return ProjectAssetService::materializeAsset(session, settings, reference, access, outLease, errorMessage);
}

bool ProjectWorkspaceService::reconcileAsset(ProjectSession& session,
                                             const creation::suite::SuiteSettings& settings,
                                             MaterializedAssetLease& lease,
                                             juce::String& errorMessage,
                                             bool releaseLeaseAfterCommit)
{
    return ProjectAssetService::reconcileAsset(session, settings, lease, errorMessage, releaseLeaseAfterCommit);
}
}
