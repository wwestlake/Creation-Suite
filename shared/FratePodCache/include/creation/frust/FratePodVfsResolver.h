#pragma once

#include <string>

#include <frate/FrateRegistryClient.h>
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
// dependencies are pinned, not ranges; frate::FrateResolver itself works
// the same way) the way any Suite app needs to: check the Suite's VFS
// cache first, fall back to the live registry on a miss, and on a
// registry hit, store the real result back into the VFS cache before
// handing it back.
//
// This is deliberately NOT frate::FrateCache -- that class is hardcoded to
// a raw local-filesystem cache root (juce::File), which is correct for
// frate used standalone outside the Suite (per the LLM/vcpkg-adjacent
// Storage Boundary Rule in AGENTS.md, that's the one thing outside the
// Suite that's still allowed to be a plain local file: frate itself is not
// a Suite app). Inside a Suite app, the durable cache of record is the
// Suite VFS, per the "Frate VFS Terminal" precedent already established
// in docs/architecture/Suite-Shared-Project-Model.md ("creates plugin
// pods in the project-owned FRust working area instead of writing to an
// arbitrary local directory").
//
// frate::FrateRegistryClient is reused as-is for the network half (it's
// already a plain HTTP client with no filesystem coupling beyond
// downloadFromS3's target-file parameter, which this class points at a
// throwaway temp file, not frate's own cache).
//
// VFS entries are opaque blobs, not something a compiler can read
// directly -- the resolved pod's actual .frpod contents are always
// extracted to a real directory on disk (localExtractRoot/<name>/<version>/,
// same shape as frate::FrateCache::getCachedPodDir) before being handed
// back, whether the pod came from the VFS cache or a fresh registry
// download. That extraction directory is a disposable local mirror of
// what the VFS holds -- the VFS entry, not this directory, is the
// durable record.
class FratePodVfsResolver {
public:
    FratePodVfsResolver(creation::services::SuiteVfsServiceClient& vfsClient,
                         frate::FrateRegistryClient& registryClient,
                         juce::File localExtractRoot);

    // On success (ResolvedFromVfsCache or ResolvedFromRegistry), outPodDir
    // is a real, already-extracted directory containing that pod's
    // frate.json and source. Untouched on any Unresolved* status.
    PodResolveStatus resolve(const std::string& name, const std::string& version,
                              juce::File& outPodDir);

    // This resolver's extraction root has the exact same <root>/<name>/
    // <version>/ shape frate::FrateCache uses -- a caller that wants
    // `frate build` itself to see pods this resolver already materialized
    // (via the FRATE_CACHE_DIR environment variable frate::FrateCache
    // already honors as its highest-priority override) can point it
    // straight here instead of re-deriving the path.
    const juce::File& localExtractRoot() const noexcept { return localExtractRoot_; }

private:
    creation::services::SuiteVfsServiceClient& vfsClient_;
    frate::FrateRegistryClient& registryClient_;
    juce::File localExtractRoot_;

    static juce::String vfsEntryPath(const std::string& name, const std::string& version);
    juce::File extractDir(const std::string& name, const std::string& version) const;
    bool extractFrpodBytes(const juce::MemoryBlock& frpodBytes, const juce::File& targetDir) const;
};

} // namespace creation::frust
