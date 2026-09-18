// Real end-to-end smoke test: writes a genuine, self-contained pod to a
// temp directory (depending on the real, live "core" pod from the
// registry -- same one FratePodVfsResolverSmoke already exercises), then
// builds it through SuiteFrateBuildService exactly the way an app would
// after codegen'ing a schematic or scaffolding a pod in an editor.

#include <creation/frust/SuiteFrateBuildService.h>

#include <iostream>

#ifndef FRATE_EXECUTABLE_PATH
#error "FRATE_EXECUTABLE_PATH must be defined by CMake to the real built frate.exe"
#endif

namespace {

bool writeTestPod(const juce::File& podDir) {
    if (!podDir.createDirectory())
        return false;
    const juce::String frateJson =
        "{\n"
        "  \"name\": \"suite_build_service_smoke_pod\",\n"
        "  \"version\": \"0.1.0\",\n"
        "  \"type\": \"lib\",\n"
        "  \"description\": \"SuiteFrateBuildService smoke test fixture.\",\n"
        "  \"dependencies\": [ { \"name\": \"core\", \"version\": \"1.0.3\" } ]\n"
        "}\n";
    if (!podDir.getChildFile("frate.json").replaceWithText(frateJson))
        return false;

    const auto srcDir = podDir.getChildFile("src");
    if (!srcDir.createDirectory())
        return false;
    const juce::String mainSource =
        "pub fn smoke_identity(x: i64) -> i64 = {\n"
        "    x\n"
        "}\n";
    return srcDir.getChildFile("lib.fr").replaceWithText(mainSource);
}

} // namespace

int main() {
    creation::services::SuiteVfsServiceClient vfsClient;
    if (!vfsClient.discover()) {
        std::cerr << "FAILED: could not discover/launch the Suite VFS service\n";
        return 1;
    }

    frate::FrateRegistryClient registryClient;
    const juce::File frateExe(FRATE_EXECUTABLE_PATH);

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getChildFile("SuiteFrateBuildServiceSmoke");
    tempRoot.deleteRecursively();
    const auto podDir = tempRoot.getChildFile("pod");
    const auto cacheMirror = tempRoot.getChildFile("cache-mirror");

    if (!writeTestPod(podDir)) {
        std::cerr << "FAILED: could not write test pod fixture\n";
        return 1;
    }

    creation::frust::SuiteFrateBuildService service(vfsClient, registryClient, frateExe, cacheMirror);
    const auto result = service.build(podDir);

    if (result.status != creation::frust::BuildStatus::Success) {
        std::cerr << "FAILED: build status " << static_cast<int>(result.status) << "\n"
                  << "frate output:\n" << result.output << "\n";
        return 1;
    }
    if (!result.builtObjectFile.existsAsFile()) {
        std::cerr << "FAILED: reported success but " << result.builtObjectFile.getFullPathName()
                  << " does not exist\n";
        return 1;
    }
    if (!cacheMirror.getChildFile("core").getChildFile("1.0.3").getChildFile("frate.json").existsAsFile()) {
        std::cerr << "FAILED: dependency was not materialized into the local cache mirror at the expected path\n";
        return 1;
    }

    tempRoot.deleteRecursively();
    std::cout << "SuiteFrateBuildService smoke test passed.\n";
    return 0;
}
