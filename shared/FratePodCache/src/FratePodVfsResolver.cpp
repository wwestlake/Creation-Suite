#include "creation/frust/FratePodVfsResolver.h"

namespace creation::frust {

FratePodVfsResolver::FratePodVfsResolver(creation::services::SuiteVfsServiceClient& vfsClient,
                                         frate::FrateRegistryClient& registryClient)
    : vfsClient_(vfsClient), registryClient_(registryClient) {}

juce::String FratePodVfsResolver::vfsEntryPath(const std::string& name, const std::string& version) {
    // Scoped under a dedicated prefix so this never collides with any
    // other suite entry (settings, AI config, ...) sharing the same flat
    // "suite/" namespace SuiteVfsServiceClient's own doc comment describes.
    return "frate-cache/" + juce::String(name) + "/" + juce::String(version) + ".frpod";
}

PodResolveStatus FratePodVfsResolver::resolve(const std::string& name, const std::string& version,
                                              frate::PodFiles& files) {
    const juce::String vfsPath = vfsEntryPath(name, version);
    std::string error;

    // The VFS entry is the durable record: a hit is always read from it.
    juce::MemoryBlock cachedBytes;
    if (vfsClient_.readEntry(vfsPath, cachedBytes)) {
        frate::PodFiles unpacked;
        if (!frate::unpackPod(cachedBytes.getData(), cachedBytes.getSize(), unpacked, error))
            return PodResolveStatus::UnresolvedExtractError;
        files = std::move(unpacked);
        return PodResolveStatus::ResolvedFromVfsCache;
    }

    // Miss: the live registry, downloaded straight into memory.
    const juce::String downloadUrl = registryClient_.getDownloadUrl(name, version);
    if (downloadUrl.isEmpty())
        return PodResolveStatus::UnresolvedNotFound;

    juce::MemoryBlock downloadedBytes;
    if (!registryClient_.downloadToMemory(downloadUrl, downloadedBytes))
        return PodResolveStatus::UnresolvedNetworkError;

    // Make sure it is a real pod before it is stored, so a bad download never
    // becomes a permanent cache entry.
    frate::PodFiles unpacked;
    if (!frate::unpackPod(downloadedBytes.getData(), downloadedBytes.getSize(), unpacked, error))
        return PodResolveStatus::UnresolvedExtractError;

    // Store it: the next resolve() for this name and version, from any Suite
    // app, is a cache hit instead of another registry request.
    if (!vfsClient_.writeEntry(vfsPath, downloadedBytes))
        return PodResolveStatus::UnresolvedNetworkError;

    files = std::move(unpacked);
    return PodResolveStatus::ResolvedFromRegistry;
}

} // namespace creation::frust
