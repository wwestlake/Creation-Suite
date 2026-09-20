// Real end-to-end smoke test, not a mock: talks to the actual running
// Suite VFS service (launched via discover() if not already running) and
// the actual live lagdaemon.com pod registry, and proves a pod is resolved,
// cached in the VFS, and returned entirely in memory.

#include <creation/frust/FratePodVfsResolver.h>

#include <frate/FrateRegistryClient.h>
#include <creation/services/SuiteVfsServiceClient.h>

#include <iostream>

int main() {
    creation::services::SuiteVfsServiceClient vfsClient;
    if (!vfsClient.discover()) {
        std::cerr << "FAILED: could not discover/launch the Suite VFS service\n";
        return 1;
    }

    frate::FrateRegistryClient registryClient;
    creation::frust::FratePodVfsResolver resolver(vfsClient, registryClient);

    const std::string podName = "frust_noise";
    const std::string podVersion = "0.1.0";
    const auto vfsEntry = creation::frust::FratePodVfsResolver::vfsEntryPath(podName, podVersion);

    // Clean slate: this exact entry must not already be cached, so the
    // first resolve() below is a genuine registry round-trip.
    vfsClient.removeEntry(vfsEntry);

    frate::PodFiles files;
    auto status = resolver.resolve(podName, podVersion, files);
    if (status != creation::frust::PodResolveStatus::ResolvedFromRegistry) {
        std::cerr << "FAILED: first resolve() expected ResolvedFromRegistry, got status "
                  << static_cast<int>(status) << "\n";
        return 1;
    }
    if (files.count("frate.json") != 1 || files.count("src/lib.fr") != 1) {
        std::cerr << "FAILED: the resolved pod has no frate.json / src/lib.fr\n";
        return 1;
    }

    juce::MemoryBlock cached;
    if (!vfsClient.readEntry(vfsEntry, cached) || cached.getSize() == 0) {
        std::cerr << "FAILED: the pod was not stored in the VFS cache\n";
        return 1;
    }

    // Second resolve() for the same name+version must now be a real VFS cache hit.
    frate::PodFiles again;
    auto secondStatus = resolver.resolve(podName, podVersion, again);
    if (secondStatus != creation::frust::PodResolveStatus::ResolvedFromVfsCache) {
        std::cerr << "FAILED: second resolve() expected ResolvedFromVfsCache, got status "
                  << static_cast<int>(secondStatus) << "\n";
        return 1;
    }
    if (again != files) {
        std::cerr << "FAILED: the cache hit returned different files\n";
        return 1;
    }

    // A pod that is not in the registry is not found, and nothing is cached for it.
    frate::PodFiles none;
    if (resolver.resolve("definitely_not_a_real_pod_name", "9.9.9", none)
            != creation::frust::PodResolveStatus::UnresolvedNotFound) {
        std::cerr << "FAILED: an unknown pod should be UnresolvedNotFound\n";
        return 1;
    }

    vfsClient.removeEntry(vfsEntry);
    std::cout << "Frate pod VFS resolver smoke test passed.\n";
    return 0;
}
