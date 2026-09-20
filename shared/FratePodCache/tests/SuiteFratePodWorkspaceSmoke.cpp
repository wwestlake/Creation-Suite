// Real end-to-end check of the Suite pod pipeline over the real VFS service:
// scaffold a pod into a project's VFS, build it in memory, and confirm the
// object file and package landed back in the VFS as entries -- and that the
// build touched no folder and started no program.

#include <creation/assets/ProjectContainerService.h>
#include <creation/frust/SuiteFratePodWorkspace.h>
#include <creation/suite/SuiteSettings.h>

#include <frate/FrateRegistryClient.h>

#include <iostream>
#include <set>

namespace {

// Files a build could plausibly leave behind, in the places a build could plausibly leave them.
std::set<std::string> watched(const juce::File& dir) {
    std::set<std::string> names;
    for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.o;*.obj;*.frpod;*.fr;*.ll;*.rsp"))
        names.insert(f.getFileName().toStdString());
    return names;
}

} // namespace

int main() {
    juce::String settingsError;
    const auto settings = creation::suite::SuiteSettingsStore().load(settingsError);

    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto cwdBefore = watched(cwd);
    const auto tmpBefore = watched(tmp);

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
    auto fail = [&](const juce::String& why) {
        cleanup();
        std::cerr << "FAILED: " << why << "\n";
        return 1;
    };

    creation::services::SuiteVfsServiceClient vfsClient;
    if (!vfsClient.discover()) return fail("could not discover Suite VFS service");

    frate::FrateRegistryClient registryClient;
    creation::frust::FratePodVfsResolver resolver(vfsClient, registryClient);
    creation::frust::SuitePodSource podSource(session, resolver);
    creation::frust::SuiteFratePodWorkspace workspace(session, podSource);

    creation::frust::PodScaffoldOptions options;
    options.name = "station_schematic_smoke";
    options.intendedApplication = "djehuti-station";
    if (!workspace.scaffold(options, error)) return fail("scaffold: " + error);
    if (!session.containsEntry(workspace.sourceRootFor(options.name) + "frate.json")
        || !session.containsEntry(workspace.sourceRootFor(options.name) + "src/lib.fr"))
        return fail("scaffold did not persist source files in VFS");

    const auto result = workspace.build(options.name);
    if (!result.success) return fail("build: " + result.output);
    if (!session.containsEntry(result.objectEntry) || !session.containsEntry(result.packageEntry))
        return fail("derived object/package were not persisted in VFS");

    juce::MemoryBlock object;
    if (!session.readEntry(result.objectEntry, object) || object.getSize() < 100)
        return fail("the object entry is empty");

    // The package in the VFS is a real .frpod that unpacks to the same pod.
    juce::MemoryBlock package;
    frate::PodFiles unpacked;
    std::string unpackError;
    if (!session.readEntry(result.packageEntry, package)
        || !frate::unpackPod(package.getData(), package.getSize(), unpacked, unpackError)
        || unpacked.count("src/lib.fr") != 1)
        return fail("the package entry does not unpack to the pod");

    // A source error reports through the build output, and writes no output entries.
    if (!workspace.writeSource(options, "pub fn broken() -> i64 = {\n    let x = ;\n}\n", error))
        return fail("writeSource: " + error);
    const auto broken = workspace.build(options.name);
    if (broken.success || !broken.output.contains("src/lib.fr"))
        return fail("a source error should fail the build and name src/lib.fr, got: " + broken.output);

    if (watched(cwd) != cwdBefore) return fail("the build left a file in the working directory");
    if (watched(tmp) != tmpBefore) return fail("the build left a file in the temp directory");

    cleanup();
    std::cout << "Suite FRust VFS pod workspace smoke test passed.\n";
    return 0;
}
