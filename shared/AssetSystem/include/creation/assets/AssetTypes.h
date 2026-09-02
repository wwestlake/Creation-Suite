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
    binary,
    pod
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

// Where an asset sits in the dependency pipeline (see
// docs/architecture/Suite-Asset-Pipeline-Model.md):
//  - root: raw imported source, read-only, no dependencies.
//  - computed: pure output of a process on its dependencies, no authored
//    content of its own -- propagates when a dependency changes, and has
//    no reason to exist once a dependency is gone.
//  - referential: its own authored content that references other assets
//    as inputs (e.g. a hand-built Material referencing Textures) --
//    survives a missing dependency in a visible missing-reference state
//    rather than being deleted or regenerated.
enum class AssetDerivationKind
{
    root,
    computed,
    referential
};

// One edge in the dependency graph: this asset depends on the given
// version of another asset. AssetDescriptor::dependencies is a SET of
// these, not a single field -- fan-out (one root feeding many dependents)
// only ever needed one link per dependent, but fan-in (a Material
// referencing several Textures at once, a Model referencing several
// Materials) needs each dependent to record all of its inputs at once.
struct AssetDependency
{
    AssetId assetId;
    AssetVersionId versionId;

    bool operator==(const AssetDependency& other) const noexcept
    {
        return assetId == other.assetId && versionId == other.versionId;
    }
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

    // Pipeline position (see AssetDerivationKind above) and, for computed/
    // referential assets, what this asset was built from. Empty for a root.
    AssetDerivationKind derivationKind = AssetDerivationKind::root;
    juce::Array<AssetDependency> dependencies;

    // For a root asset only: the external OS filesystem path it was
    // imported from, remembered so Reimport/Update can re-read it later
    // without the user having to browse for it again. If the file has
    // moved or been deleted by the time of a reimport, the user is
    // offered a file-browse dialog to relocate it -- the located path
    // then replaces this field going forward. Empty for anything that
    // isn't a root (a computed/referential asset has no external file of
    // its own to reimport from).
    juce::String externalSourcePath;

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
juce::String toStorageToken(AssetDerivationKind kind);
AssetDerivationKind assetDerivationKindFromStorageToken(const juce::String& token);
}
