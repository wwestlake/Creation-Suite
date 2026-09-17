#include <creation/assets/ProjectContainerService.h>
#include <creation/frust/SuiteFratePodWorkspace.h>
#include <creation/suite/SuiteSettings.h>

#include <frate/FrateRegistryClient.h>

#include <iostream>

#ifndef FRATE_EXECUTABLE_PATH
#error "FRATE_EXECUTABLE_PATH must point to frate.exe"
#endif

int main() {
    juce::String settingsError;
    const auto settings = creation::suite::SuiteSettingsStore().load(settingsError);

    creation::assets::ProjectSession session;
    juce::String error;
    if (!creation::assets::ProjectContainerService::createProject(
            settings, creation::assets::SuiteAppDomain::station,
            "FRust Pod Workspace Smoke", "test", "test", session, error)) {
        std::cerr << "FAILED: could not create VFS test project: " << error << "\n";
        return 1;
    }

    const auto projectId = session.getProjectId();
    auto cleanup = [&] {
        juce::String ignored;
        creation::assets::ProjectContainerService::deleteProject(settings, projectId, ignored);
    };

    creation::services::SuiteVfsServiceClient vfsClient;
    if (!vfsClient.discover()) {
        cleanup();
        std::cerr << "FAILED: could not discover Suite VFS service\n";
        return 1;
    }
    frate::FrateRegistryClient registryClient;
    const auto workRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("SuiteFratePodWorkspaceSmoke");
    workRoot.deleteRecursively();
    creation::frust::SuiteFrateBuildService buildService(
        vfsClient, registryClient, juce::File(FRATE_EXECUTABLE_PATH), workRoot.getChildFile("cache"));
    creation::frust::SuiteFratePodWorkspace workspace(session, buildService, workRoot.getChildFile("pods"));

    creation::frust::PodScaffoldOptions options;
    options.name = "station_schematic_smoke";
    options.intendedApplication = "djehuti-station";
    if (!workspace.scaffold(options, error)) {
        cleanup();
        std::cerr << "FAILED: scaffold: " << error << "\n";
        return 1;
    }
    if (!session.containsEntry(workspace.sourceRootFor(options.name) + "frate.json")
        || !session.containsEntry(workspace.sourceRootFor(options.name) + "src/lib.fr")) {
        cleanup();
        std::cerr << "FAILED: scaffold did not persist source files in VFS\n";
        return 1;
    }

    const auto result = workspace.build(options.name);
    if (result.build.status != creation::frust::BuildStatus::Success) {
        cleanup();
        std::cerr << "FAILED: build: " << result.build.output << "\n";
        return 1;
    }
    if (!session.containsEntry(result.derivedRoot + "build/station_schematic_smoke.o")
        || result.packageEntry.isEmpty() || !session.containsEntry(result.packageEntry)) {
        cleanup();
        std::cerr << "FAILED: derived object/package were not persisted in VFS\n";
        return 1;
    }

    cleanup();
    workRoot.deleteRecursively();
    std::cout << "Suite FRust VFS pod workspace smoke test passed.\n";
    return 0;
}
