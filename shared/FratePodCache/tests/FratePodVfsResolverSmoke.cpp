// Real end-to-end smoke test, not a mock: talks to the actual running
// Suite VFS service (launched via discover() if not already running) and
// the actual live lagdaemon.com pod registry. Mirrors this session's own
// standard (see AudioGraphSelfTest.cpp) of proving the real pipeline
// works rather than asserting against a stand-in.

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
    juce::File extractRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("FratePodVfsResolverSmoke");
    extractRoot.deleteRecursively();

    creation::frust::FratePodVfsResolver resolver(vfsClient, registryClient, extractRoot);

    const std::string podName = "frust_noise";
    const std::string podVersion = "0.1.0";
    const juce::String vfsEntry = "frate-cache/" + juce::String(podName) + "/" + juce::String(podVersion) + ".frpod";

    // Clean slate: this exact entry must not already be cached, so the
    // first resolve() below is a genuine registry round-trip, not an
    // accidental hit from a previous run.
    vfsClient.removeEntry(vfsEntry);

    juce::File podDir;
    auto status = resolver.resolve(podName, podVersion, podDir);
    if (status != creation::frust::PodResolveStatus::ResolvedFromRegistry) {
        std::cerr << "FAILED: first resolve() expected ResolvedFromRegistry, got status "
                  << static_cast<int>(status) << "\n";
        return 1;
    }
    if (!podDir.getChildFile("frate.json").existsAsFile()) {
        std::cerr << "FAILED: resolved pod directory has no frate.json: "
                  << podDir.getFullPathName() << "\n";
        return 1;
    }

    // Second resolve() for the exact same name+version must now be a real
    // VFS cache hit -- the whole point of this class.
    juce::File podDirAgain;
    auto secondStatus = resolver.resolve(podName, podVersion, podDirAgain);
    if (secondStatus != creation::frust::PodResolveStatus::ResolvedFromVfsCache) {
        std::cerr << "FAILED: second resolve() expected ResolvedFromVfsCache, got status "
                  << static_cast<int>(secondStatus) << "\n";
        return 1;
    }
    if (!podDirAgain.getChildFile("frate.json").existsAsFile()) {
        std::cerr << "FAILED: cache-hit resolved pod directory has no frate.json\n";
        return 1;
    }

    // A pod that genuinely doesn't exist must fail cleanly, not silently
    // succeed with garbage.
    juce::File missingPodDir;
    auto missingStatus = resolver.resolve("this_pod_does_not_exist_12345", "0.0.0", missingPodDir);
    if (missingStatus != creation::frust::PodResolveStatus::UnresolvedNotFound) {
        std::cerr << "FAILED: nonexistent pod expected UnresolvedNotFound, got status "
                  << static_cast<int>(missingStatus) << "\n";
        return 1;
    }

    vfsClient.removeEntry(vfsEntry);
    extractRoot.deleteRecursively();

    std::cout << "FratePodVfsResolver smoke test passed.\n";
    return 0;
}
