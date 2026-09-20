// End-to-end check of SuiteFrust over the REAL VFS service: the compiler and Frate work on the
// VFS as their file system and log, and nothing lands on the operating system's disk.

#include <creation/assets/ProjectContainerService.h>
#include <creation/frust/SuiteFrust.h>
#include <creation/suite/SuiteSettings.h>

#include <frate/PodEnvironment.h>

#include <iostream>
#include <set>

namespace {

int failures = 0;

void check(bool ok, const juce::String& what) {
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (!ok) ++failures;
}

std::set<std::string> watched(const juce::File& dir) {
    std::set<std::string> names;
    for (const auto& f : dir.findChildFiles(juce::File::findFiles, false, "*.o;*.obj;*.frpod;*.fr;*.frust;*.ll;*.rsp;*.log"))
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
            settings, creation::assets::SuiteAppDomain::station, "SuiteFrust Smoke", "test", "test", session, error)) {
        std::cerr << "FAILED: could not create a VFS test project: " << error << "\n";
        return 1;
    }
    const auto projectId = session.getProjectId();
    auto cleanup = [&] {
        juce::String ignored;
        creation::assets::ProjectContainerService::deleteProject(settings, projectId, ignored);
    };

    creation::services::SuiteVfsServiceClient vfs;
    if (!vfs.discover()) {
        cleanup();
        std::cerr << "FAILED: could not discover the Suite VFS service\n";
        return 1;
    }

    frate::FrateRegistryClient registry;
    creation::frust::SuiteFrust frust(session, vfs, registry);

    // ---- Loose source (the Script panel path) ----
    {
        const auto good = frust.checkSource("Assets/Source/FRust/Scripts/patch.frust",
                                            "pub fn gain(input: f64, amount: f64) -> f64 = {\n    input * amount\n}\n");
        check(good.ok && good.output == "Compiled cleanly.", "a good script compiles");
        check(session.containsEntry("Assets/Source/FRust/Scripts/patch.frust"), "the script is stored in the project VFS");

        const auto bad = frust.checkSource("Assets/Source/FRust/Scripts/bad.frust", "pub fn f() -> i64 = {\n    let x = ;\n}\n");
        check(!bad.ok && bad.output.contains("bad.frust:2:"), "a bad script reports the virtual path and line");
    }

    // ---- The log is in the VFS ----
    {
        juce::String logEntry;
        for (const auto& entry : session.listEntryPaths())
            if (entry.startsWith("Logs/frust/") && entry.endsWith(".log")) logEntry = entry;
        juce::MemoryBlock logBytes;
        check(logEntry.isNotEmpty() && session.readEntry(logEntry, logBytes), "a log entry exists in the project VFS");
        const auto text = juce::String::fromUTF8(static_cast<const char*>(logBytes.getData()), (int) logBytes.getSize());
        check(text.contains("compiled 1 source(s)") && text.contains("compilation failed"),
              "the log holds the compile results");
    }

    // ---- An authored pod: scaffold, build, package ----
    creation::frust::PodScaffoldOptions options;
    options.name = "suite_frust_smoke";
    options.exposeAsNodeLibrary = true;
    check(frust.scaffoldPod(options, error), "scaffold a pod: " + error);
    check(session.containsEntry("Assets/Source/FRust/PluginPods/suite_frust_smoke/frate.json")
              && session.containsEntry("Assets/Source/FRust/PluginPods/suite_frust_smoke/src/lib.fr"),
          "the pod's files are VFS entries");

    const auto built = frust.buildPod("suite_frust_smoke");
    check(built.ok, "the pod builds: " + built.output);
    juce::MemoryBlock object;
    check(session.readEntry("Assets/Source/FRust/PluginPods/suite_frust_smoke/build/suite_frust_smoke.o", object)
              && object.getSize() > 100,
          "the object file is a VFS entry");

    const auto packaged = frust.packagePod("suite_frust_smoke");
    check(packaged.ok && session.containsEntry("Assets/Source/FRust/PluginPods/suite_frust_smoke/dist/suite_frust_smoke-0.1.0.frpod"),
          "the package is a VFS entry");

    // A source error is reported and writes no object.
    check(frust.writePodSource(options, "pub fn broken() -> i64 = {\n    let x = ;\n}\n", error), "write a broken source");
    const auto broken = frust.buildPod("suite_frust_smoke");
    check(!broken.ok && broken.output.contains("src/lib.fr"), "a broken pod reports its file");

    // ---- Install a package into the Suite pod store, then build a pod that uses it ----
    {
        auto env = frust.environment();
        std::string packageBytes;
        check(env.files.read("Assets/Source/FRust/PluginPods/suite_frust_smoke/dist/suite_frust_smoke-0.1.0.frpod", packageBytes),
              "read the package back");
        std::string name, version, installError;
        check(frate::installPodPackage(env, "pods", packageBytes, name, version, installError)
                  && name == "suite_frust_smoke" && version == "0.1.0",
              "install the package into the Suite pod store");
        check(vfs.readEntry("frate-pods/suite_frust_smoke/0.1.0/frate.json", *std::make_unique<juce::MemoryBlock>()),
              "the installed pod is in the Suite-level VFS");

        env.files.write("Assets/Source/FRust/PluginPods/consumer/frate.json",
                        "{\"name\":\"consumer\",\"version\":\"0.1.0\",\"type\":\"lib\",\"dependencies\":[{\"name\":\"suite_frust_smoke\",\"version\":\"0.1.0\"}]}");
        env.files.write("Assets/Source/FRust/PluginPods/consumer/src/lib.fr", "use suite_frust_smoke;\npub fn f() -> i64 = 1\n");
        const auto consumer = frust.buildPod("consumer");
        check(consumer.ok, "a pod that uses an installed pod builds: " + consumer.output);
    }

    // ---- A registry pod, straight into memory (needs the network) ----
    {
        vfs.removeEntry("frate-pods/frust_noise/0.1.0/frate.json");
        const bool installed = frust.installRegistryPod("frust_noise", "0.1.0", error);
        check(installed, "a registry pod installs into the Suite pod store: " + error);
    }

    frust.flushLog();
    check(watched(cwd) == cwdBefore, "nothing was left in the working directory");
    check(watched(tmp) == tmpBefore, "nothing was left in the temp directory");

    cleanup();
    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + juce::String(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
