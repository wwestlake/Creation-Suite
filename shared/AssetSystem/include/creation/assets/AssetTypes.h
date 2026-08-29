#pragma once

#include <cstdint>
#include <juce_core/juce_core.h>
#include <optional>

namespace creation::assets
{
using AssetId = juce::String;
using AssetVersionId = juce::String;

enum class AssetKind
{
    unknown,
    audio,
    render,
    patch,
    foleyPatch,
    trackerArrangement,
    script,
    metadata,
    preset,
    samplePack,
    midi,
    binary
};

enum class AssetReferenceMode
{
    exact,
    compatibleLatest,
    latest
};

struct AssetRef
{
    AssetId id;
    AssetVersionId versionId;
    AssetReferenceMode mode = AssetReferenceMode::exact;
    juce::String logicalPath;
    juce::String displayName;

    bool isValid() const noexcept { return id.trim().isNotEmpty(); }
};

struct AssetDescriptor
{
    AssetId id;
    AssetVersionId versionId;
    AssetId originalAssetId;
    AssetVersionId derivedFromVersionId;
    juce::String displayName;
    AssetKind kind = AssetKind::unknown;
    juce::String category;
    juce::String description;
    juce::String mediaType;
    juce::String logicalPath;
    juce::String sourceApp;
    juce::String sourceTool;
    // Provenance for generated assets. A derived asset points to the exact source
    // revision used to create it; these remain optional for legacy assets.
    AssetId sourceAssetId;
    AssetVersionId sourceVersionId;
    juce::String importerId;
    juce::String importerVersion;
    juce::String importSettings;
    juce::String version { "1" };
    juce::StringArray tags;
    int64_t fileSizeBytes = 0;
    int revision = 0;
    juce::Time createdAt;
    juce::Time modifiedAt;
};

struct AssetQuery
{
    std::optional<AssetKind> kind;
    juce::String logicalPathPrefix;
    juce::String searchText;
    juce::StringArray tags;
};

juce::String toStorageToken(AssetKind kind);
AssetKind assetKindFromStorageToken(const juce::String& token);
juce::String toDisplayName(AssetKind kind);
juce::String toStorageToken(AssetReferenceMode mode);
AssetReferenceMode assetReferenceModeFromStorageToken(const juce::String& token);
}
