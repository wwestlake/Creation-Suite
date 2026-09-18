#include "creation/frust/FratePodVfsResolver.h"

namespace creation::frust {

FratePodVfsResolver::FratePodVfsResolver(creation::services::SuiteVfsServiceClient& vfsClient,
                                          frate::FrateRegistryClient& registryClient,
                                          juce::File localExtractRoot)
    : vfsClient_(vfsClient), registryClient_(registryClient), localExtractRoot_(std::move(localExtractRoot)) {}

juce::String FratePodVfsResolver::vfsEntryPath(const std::string& name, const std::string& version) {
    // Scoped under a dedicated prefix so this never collides with any
    // other suite entry (settings, AI config, ...) sharing the same flat
    // "suite/" namespace SuiteVfsServiceClient's own doc comment describes.
    return "frate-cache/" + juce::String(name) + "/" + juce::String(version) + ".frpod";
}

juce::File FratePodVfsResolver::extractDir(const std::string& name, const std::string& version) const {
    // Same directory shape as frate::FrateCache::getCachedPodDir, just
    // rooted under this resolver's own localExtractRoot instead of
    // frate's raw cache root -- the two are deliberately kept separate
    // (see this class's header comment).
    return localExtractRoot_.getChildFile(juce::String(name)).getChildFile(juce::String(version));
}

bool FratePodVfsResolver::extractFrpodBytes(const juce::MemoryBlock& frpodBytes, const juce::File& targetDir) const {
    // A .frpod is a real zip file (pod.json + source) -- mirrors
    // frate::FrateCache::installFromPackage's own extraction exactly,
    // just from an in-memory blob instead of a file already on disk.
    juce::File tempZip = juce::File::createTempFile(".frpod");
    if (!tempZip.replaceWithData(frpodBytes.getData(), frpodBytes.getSize())) {
        return false;
    }

    if (!targetDir.exists()) {
        targetDir.createDirectory();
    }

    juce::ZipFile zip(tempZip);
    auto result = zip.uncompressTo(targetDir);
    tempZip.deleteFile();
    return result.wasOk();
}

PodResolveStatus FratePodVfsResolver::resolve(const std::string& name, const std::string& version,
                                               juce::File& outPodDir) {
    const juce::File target = extractDir(name, version);
    const juce::String vfsPath = vfsEntryPath(name, version);

    // Cache check: the VFS entry, not the local extraction directory, is
    // the durable record -- always re-derived from the VFS blob when it
    // exists, even if a stale local extraction directory is already
    // sitting there from a previous run.
    juce::MemoryBlock cachedBytes;
    if (vfsClient_.readEntry(vfsPath, cachedBytes)) {
        if (!extractFrpodBytes(cachedBytes, target)) {
            return PodResolveStatus::UnresolvedExtractError;
        }
        outPodDir = target;
        return PodResolveStatus::ResolvedFromVfsCache;
    }

    // Cache miss: fall back to the live registry, reusing
    // frate::FrateRegistryClient exactly as frate::FrateResolver does.
    const juce::String downloadUrl = registryClient_.getDownloadUrl(name, version);
    if (downloadUrl.isEmpty()) {
        return PodResolveStatus::UnresolvedNotFound;
    }

    juce::File tempDownload = juce::File::createTempFile(".frpod");
    if (!registryClient_.downloadFromS3(downloadUrl, tempDownload)) {
        tempDownload.deleteFile();
        return PodResolveStatus::UnresolvedNetworkError;
    }

    juce::MemoryBlock downloadedBytes;
    if (!tempDownload.loadFileAsData(downloadedBytes)) {
        tempDownload.deleteFile();
        return PodResolveStatus::UnresolvedExtractError;
    }
    tempDownload.deleteFile();

    // Store the real result in the VFS cache -- this is what makes the
    // next resolve() for this exact name+version a cache hit instead of
    // hitting the registry again, from any Suite app, not just this
    // process.
    if (!vfsClient_.writeEntry(vfsPath, downloadedBytes)) {
        return PodResolveStatus::UnresolvedNetworkError;
    }

    if (!extractFrpodBytes(downloadedBytes, target)) {
        return PodResolveStatus::UnresolvedExtractError;
    }

    outPodDir = target;
    return PodResolveStatus::ResolvedFromRegistry;
}

} // namespace creation::frust
