#include "creation/assets/AssetResolver.h"

namespace creation::assets
{
const AssetDescriptor* AssetResolver::resolve(const AssetCatalog& catalog, const AssetRef& reference) noexcept
{
    if (! reference.isValid())
        return nullptr;

    if (reference.mode == AssetReferenceMode::exact)
    {
        if (reference.versionId.isNotEmpty())
            return catalog.findByVersionId(reference.versionId);
        return catalog.findById(reference.id);
    }

    const auto versions = catalog.findAllVersions(reference.id);
    if (versions.isEmpty())
        return nullptr;

    if (reference.mode == AssetReferenceMode::latest)
    {
        for (const auto& candidate : versions)
            if (const auto* match = catalog.findByVersionId(candidate.versionId))
                return match;
        return nullptr;
    }

    if (reference.versionId.isNotEmpty())
    {
        const auto* exact = catalog.findByVersionId(reference.versionId);
        if (exact != nullptr)
            return exact;
    }

    const auto requestedLogicalPath = reference.logicalPath.trim();
    if (requestedLogicalPath.isNotEmpty())
    {
        for (const auto& candidate : versions)
            if (candidate.logicalPath == requestedLogicalPath)
                if (const auto* match = catalog.findByVersionId(candidate.versionId))
                    return match;
    }

    for (const auto& candidate : versions)
        if (const auto* match = catalog.findByVersionId(candidate.versionId))
            return match;

    return nullptr;
}
}
