#include "creation/frust/SuiteFrust.h"

#include <frate/FrateConfig.h>
#include <frate/PodArchive.h>
#include <frate/PodEnvironment.h>

#include <EnvironmentCompile.h>

namespace creation::frust {
namespace {

const juce::String kPodsRoot = "pods";                            // the Suite pod store, virtual path
const juce::String kPodSourceRoot = "Assets/Source/FRust/PluginPods/";

juce::String jsonString(const juce::String& value) {
    return juce::JSON::toString(juce::var(value), false);
}

// Flushes the log when an operation ends, however it ends.
struct FlushOnExit {
    VfsLogSink& log;
    ~FlushOnExit() { log.flush(); }
};

juce::String diagnosticsText(const std::vector<::frust::Diagnostic>& diagnostics) {
    juce::String text;
    for (const auto& d : diagnostics) text << juce::String(::frust::FormatDiagnostic(d)) << "\n";
    return text;
}

} // namespace

SuiteFrust::SuiteFrust(creation::assets::ProjectSession& project,
                       creation::services::SuiteVfsServiceClient& suiteVfs,
                       frate::FrateRegistryClient& registry)
    : project_(project), registry_(registry), files_(project, suiteVfs), log_(project) {}

// ---- Loose source ----

FrustOutcome SuiteFrust::checkSource(const juce::String& virtualPath, const juce::String& text) {
    FlushOnExit flush { log_ };
    FrustOutcome outcome;
    if (!project_.isValid()) {
        outcome.output = "Open or create a project first: FRust source is kept in the project.";
        return outcome;
    }

    auto env = environment();
    if (!files_.write(virtualPath.toStdString(), text.toStdString())) {
        outcome.output = "Could not store the source in the project.";
        return outcome;
    }

    ::frust::FileCompileRequest request;
    request.sources = { virtualPath.toStdString() };
    request.podsRoot = kPodsRoot.toStdString();
    const auto result = ::frust::compileFiles(env, request);

    outcome.ok = result.ok;
    outcome.output = result.ok ? juce::String("Compiled cleanly.") : diagnosticsText(result.diagnostics);
    if (outcome.output.isEmpty()) outcome.output = "The FRust compiler reported a failure without a message.";
    return outcome;
}

// ---- Authored pods ----

juce::String SuiteFrust::podRootFor(const juce::String& podName) {
    return kPodSourceRoot + podName;
}

bool SuiteFrust::validPodName(const juce::String& name) {
    if (name.isEmpty()) return false;
    for (const auto character : name)
        if (!juce::CharacterFunctions::isLetterOrDigit(character) && character != '-' && character != '_')
            return false;
    return name == name.toLowerCase();
}

bool SuiteFrust::scaffoldPod(const PodScaffoldOptions& options, juce::String& error) {
    FlushOnExit flush { log_ };
    const auto name = options.name.trim();
    if (!project_.isValid()) {
        error = "Open a Suite project before creating a FRust pod.";
        return false;
    }
    if (!validPodName(name)) {
        error = "Pod names use lowercase letters, numbers, '-' and '_' only.";
        return false;
    }
    const auto root = podRootFor(name);
    if (files_.exists((root + "/frate.json").toStdString())) {
        error = "A pod named '" + name + "' already exists in this project.";
        return false;
    }

    const auto manifest = "{\n"
        "  \"name\": " + jsonString(name) + ",\n"
        "  \"version\": " + jsonString(options.version) + ",\n"
        "  \"type\": \"lib\",\n"
        "  \"description\": " + jsonString(options.description) + ",\n"
        "  \"dependencies\": []\n"
        "}\n";

    auto source = "manifest \"{\\\"name\\\":\\\"" + name
        + "\\\",\\\"version\\\":\\\"" + options.version
        + "\\\",\\\"intendedApplications\\\":[\\\"" + options.intendedApplication + "\\\"]}\";\n\n";
    if (options.exposeAsNodeLibrary)
        source += "node pure fn identity(value: f64) -> f64 = {\n    value\n}\n";
    else
        source += "pub fn on_init() -> i64 = {\n    0\n}\n";

    if (!files_.write((root + "/frate.json").toStdString(), manifest.toStdString())
        || !files_.write((root + "/src/lib.fr").toStdString(), source.toStdString())) {
        error = "Could not write the pod into the project VFS.";
        return false;
    }
    log_.log(::frust::LogLevel::Info, "frate", ("created pod " + name + " at " + root).toStdString());
    return true;
}

bool SuiteFrust::writePodSource(const PodScaffoldOptions& options, const juce::String& source, juce::String& error) {
    const auto name = options.name.trim();
    if (!files_.exists((podRootFor(name) + "/frate.json").toStdString()) && !scaffoldPod(options, error))
        return false;
    FlushOnExit flush { log_ };
    if (!files_.write((podRootFor(name) + "/src/lib.fr").toStdString(), source.toStdString())) {
        error = "Could not write the FRust source into the project VFS.";
        return false;
    }
    return true;
}

FrustOutcome SuiteFrust::buildPod(const juce::String& podName) {
    FlushOnExit flush { log_ };
    FrustOutcome outcome;
    if (!project_.isValid() || !validPodName(podName)) {
        outcome.output = "Open a project and use a valid pod name.";
        return outcome;
    }

    auto env = environment();
    frate::PodEnvironmentBuildOptions options;
    options.podRoot = podRootFor(podName).toStdString();
    options.podsRoot = kPodsRoot.toStdString();
    const auto built = frate::buildPodInEnvironment(env, options);

    outcome.ok = built.ok;
    outcome.output = built.ok ? "Built " + juce::String(built.name) + " " + juce::String(built.version) + "."
                              : diagnosticsText(built.diagnostics);
    if (outcome.output.isEmpty()) outcome.output = "The build failed without a message.";
    return outcome;
}

FrustOutcome SuiteFrust::packagePod(const juce::String& podName) {
    FlushOnExit flush { log_ };
    FrustOutcome outcome;
    auto env = environment();
    std::string path, error;
    outcome.ok = frate::packPodInEnvironment(env, podRootFor(podName).toStdString(), path, error);
    outcome.output = outcome.ok ? "Packaged " + juce::String(path) : juce::String(error);
    return outcome;
}

// ---- Registry pods ----

bool SuiteFrust::installRegistryPod(const juce::String& name, const juce::String& version, juce::String& error) {
    FlushOnExit flush { log_ };
    return installRegistryPodRecursive(name, version, 0, error);
}

bool SuiteFrust::installRegistryPodRecursive(const juce::String& name, const juce::String& version, int depth,
                                             juce::String& error) {
    if (depth > 8) {
        error = "Pod dependencies are nested too deeply.";
        return false;
    }

    const std::string root = ::frust::JoinPath(::frust::JoinPath(kPodsRoot.toStdString(), name.toStdString()),
                                               version.toStdString());
    std::string manifest;
    if (!files_.read(::frust::JoinPath(root, "frate.json"), manifest)) {
        // Not in the Suite pod store yet: download straight into memory and install through the environment.
        const auto url = registry_.getDownloadUrl(name.toStdString(), version.toStdString());
        if (url.isEmpty()) {
            error = "Could not find " + name + " " + version + " in the Frate registry.";
            return false;
        }
        juce::MemoryBlock bytes;
        if (!registry_.downloadToMemory(url, bytes)) {
            error = "Could not download " + name + " " + version + " from the Frate registry.";
            return false;
        }
        auto env = environment();
        std::string installedName, installedVersion, installError;
        const std::string package(static_cast<const char*>(bytes.getData()), bytes.getSize());
        if (!frate::installPodPackage(env, kPodsRoot.toStdString(), package, installedName, installedVersion, installError)) {
            error = "Could not install " + name + " " + version + ": " + juce::String(installError);
            return false;
        }
        if (juce::String(installedName) != name || juce::String(installedVersion) != version) {
            error = "The registry returned " + juce::String(installedName) + " " + juce::String(installedVersion)
                    + " for " + name + " " + version + ".";
            return false;
        }
        files_.read(::frust::JoinPath(root, "frate.json"), manifest);
    }

    // Its dependencies come along.
    frate::FrateConfig config;
    if (config.loadFromString(manifest)) {
        for (const auto& dep : config.getDependencies()) {
            if (!installRegistryPodRecursive(juce::String(dep.name), juce::String(dep.version), depth + 1, error))
                return false;
        }
    }
    return true;
}

// ---- Running ----

bool SuiteFrust::loadEntry(PluginRuntime& runtime, const juce::String& key, const juce::String& podRoot,
                           const std::string& podsRoot, juce::String& error) {
    FlushOnExit flush { log_ };
    frate::PodFiles files;
    std::string readError;
    auto env = environment();
    if (!frate::readPodFiles(env, podRoot.toStdString(), files, readError)) {
        error = juce::String(readError);
        return false;
    }

    frate::PodEntry entry;
    if (!frate::findPodEntry(files, entry, readError)) {
        error = juce::String(readError);
        return false;
    }

    frate::FrateConfig config;
    config.loadFromString(files["frate.json"]);

    ::frust::FileCompileRequest request;
    request.sources = { ::frust::JoinPath(podRoot.toStdString(), entry.name) };
    request.podsRoot = podsRoot;
    for (const auto& dep : config.getDependencies()) request.dependencies.push_back({ dep.name, dep.version });

    std::string loadError;
    runtime.unload(key.toStdString());
    if (!runtime.loadFromEnvironment(key.toStdString(), env, request, loadError)) {
        error = juce::String(loadError);
        log_.log(::frust::LogLevel::Error, "frust", loadError);
        return false;
    }
    log_.log(::frust::LogLevel::Info, "frust", ("loaded " + podRoot + " as " + key).toStdString());
    return true;
}

bool SuiteFrust::loadAuthoredPod(PluginRuntime& runtime, const juce::String& key, const juce::String& podName,
                                 juce::String& error) {
    return loadEntry(runtime, key, podRootFor(podName), kPodsRoot.toStdString(), error);
}

bool SuiteFrust::loadInstalledPod(PluginRuntime& runtime, const juce::String& key, const juce::String& name,
                                  const juce::String& version, juce::String& error) {
    const auto root = kPodsRoot + "/" + name + "/" + version;
    return loadEntry(runtime, key, root, kPodsRoot.toStdString(), error);
}

} // namespace creation::frust
