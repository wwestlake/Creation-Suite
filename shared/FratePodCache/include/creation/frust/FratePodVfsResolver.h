#pragma once

#include <string>

#include <frate/FrateRegistryClient.h>
#include <frate/PodArchive.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <juce_core/juce_core.h>

namespace creation::frust {

enum class PodResolveStatus {
    ResolvedFromVfsCache,
    ResolvedFromRegistry,
    UnresolvedNotFound,
    UnresolvedNetworkError,
    UnresolvedExtractError,
};

// Resolves a pod dependency (exact name + exact version -- frate.json
// dependencies are pinned, not ranges) the way any Suite app needs to: check
// the Suite VFS first, fall back to the live registry on a miss, and on a
// registry hit, store the .frpod in the VFS before handing the pod back.
//
// The pod comes back as frate::PodFiles -- its files held in memory. Nothing
// is extracted to a folder, no temp file is created, and the registry download
// goes straight into memory: the VFS entry is the only place a pod ever lives.
// Frate's own command-line cache (frate::FrateCache) is a different thing; it
// belongs to the terminal tool, not to the Suite.
class FratePodVfsResolver {
public:
    FratePodVfsResolver(creation::services::SuiteVfsServiceClient& vfsClient,
                        frate::FrateRegistryClient& registryClient);

    // On success (ResolvedFromVfsCache or ResolvedFromRegistry), `files` holds
    // the pod. Untouched on any Unresolved* status.
    PodResolveStatus resolve(const std::string& name, const std::string& version, frate::PodFiles& files);

    // Where a pod's .frpod lives in the Suite VFS.
    static juce::String vfsEntryPath(const std::string& name, const std::string& version);

private:
    creation::services::SuiteVfsServiceClient& vfsClient_;
    frate::FrateRegistryClient& registryClient_;
};

} // namespace creation::frust
