#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/ProjectSession.h"
#include "creation/assets/AssetResolver.h"

namespace creation::assets
{
class ProjectAssetService final
{
public:
    struct ImportOptions
    {
        AssetKind kind = AssetKind::unknown;
        juce::String displayName;
        juce::String logicalPath;
        juce::String category;
        juce::String description;
        juce::String mediaType;
        juce::String sourceApp;
        juce::String sourceTool;
        AssetId sourceAssetId;
        AssetVersionId sourceVersionId;
        juce::String importerId;
        juce::String importerVersion;
        juce::String importSettings;
        juce::StringArray tags;
        AssetId originalAssetId;
        AssetVersionId derivedFromVersionId;
        int compressionLevel = 9;
    };

    static bool importFile(ProjectSession& session,
                           const juce::File& sourceFile,
                           const ImportOptions& options,
                           AssetDescriptor& outDescriptor,
                           juce::String& errorMessage);

    // Same shape as importFile, for tool-generated content that only ever exists in memory (a
    // serialized arrangement, patch, or node graph) -- never touches disk outside the project
    // itself. options.logicalPath is required (there's no source file to derive a default from).
    // A save whose logicalPath matches an existing asset reuses that asset's id and bumps its
    // revision (silent overwrite by name, matching the convention already shipped for Signal Lab
    // patches); otherwise a fresh id is minted.
    static bool saveGeneratedAsset(ProjectSession& session,
                                   const juce::MemoryBlock& data,
                                   const ImportOptions& options,
                                   AssetDescriptor& outDescriptor,
                                   juce::String& errorMessage);

    static bool createNewVersion(ProjectSession& session,
                                 const AssetDescriptor& existingAsset,
                                 const juce::File& sourceFile,
                                 const ImportOptions& overrides,
                                 AssetDescriptor& outDescriptor,
                                 juce::String& errorMessage);

    static const AssetDescriptor* resolveAsset(const ProjectSession& session,
                                               const AssetRef& reference) noexcept;

    static bool materializeAsset(const ProjectSession& session,
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
