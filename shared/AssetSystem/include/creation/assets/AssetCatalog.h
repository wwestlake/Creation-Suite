#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/AssetTypes.h"

namespace creation::assets
{
struct AssetCatalog
{
    juce::Array<AssetDescriptor> assets;

    const AssetDescriptor* findById(const AssetId& assetId) const noexcept;
    const AssetDescriptor* findByVersionId(const AssetVersionId& versionId) const noexcept;
    juce::Array<AssetDescriptor> findAllVersions(const AssetId& assetId) const;
    juce::Array<AssetDescriptor> query(const AssetQuery& query) const;

    // Every descriptor (any version, any asset) that lists assetId in its
    // own AssetDescriptor::dependencies -- the delete-with-dependency-
    // check operation from the pipeline model (see
    // docs/architecture/Suite-Asset-Pipeline-Model.md) reads this before
    // allowing a delete to proceed. Returns empty until something actually
    // populates dependencies on import/derive -- today's importers don't
    // yet, so this is a currently-inert safeguard, not dead code: it's
    // meant to start mattering the moment a referential asset (e.g. a
    // persisted Material) records what it depends on.
    juce::Array<AssetDescriptor> findDependents(const AssetId& assetId) const;
};

juce::var toVar(const AssetDescriptor& descriptor);
bool fromVar(const juce::var& value, AssetDescriptor& outDescriptor);

juce::var toVar(const AssetCatalog& catalog);
bool fromVar(const juce::var& value, AssetCatalog& outCatalog);
}
