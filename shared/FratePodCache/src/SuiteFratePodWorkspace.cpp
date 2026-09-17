#include "creation/frust/SuiteFratePodWorkspace.h"

#include <frate/FratePodBuilder.h>

namespace creation::frust {
namespace {

juce::MemoryBlock bytes(const juce::String& text) {
    return { text.toRawUTF8(), static_cast<size_t>(text.getNumBytesAsUTF8()) };
}

juce::String jsonString(const juce::String& value) {
    return juce::JSON::toString(juce::var(value), false);
}

} // namespace

SuiteFratePodWorkspace::SuiteFratePodWorkspace(creation::assets::ProjectSession& session,
                                               SuiteFrateBuildService& buildService,
                                               juce::File materializationRoot)
    : session_(session), buildService_(buildService), materializationRoot_(std::move(materializationRoot)) {}

juce::String SuiteFratePodWorkspace::sourceRootFor(const juce::String& podName) {
    return "Assets/Source/FRust/PluginPods/" + podName + "/";
}

juce::String SuiteFratePodWorkspace::derivedRootFor(const juce::String& podName) {
    return "Assets/Derived/FRust/PluginPods/" + podName + "/";
}

bool SuiteFratePodWorkspace::validPodName(const juce::String& name) {
    if (name.isEmpty()) return false;
    for (const auto character : name)
        if (!juce::CharacterFunctions::isLetterOrDigit(character) && character != '-' && character != '_')
            return false;
    return name == name.toLowerCase();
}

bool SuiteFratePodWorkspace::scaffold(const PodScaffoldOptions& options, juce::String& error) {
    const auto name = options.name.trim();
    if (!session_.isValid()) {
        error = "Open a Suite project before creating a FRust pod.";
        return false;
    }
    if (!validPodName(name)) {
        error = "Pod names use lowercase letters, numbers, '-' and '_' only.";
        return false;
    }
    const auto root = sourceRootFor(name);
    if (session_.containsEntry(root + "frate.json")) {
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
    if (options.exposeAsNodeLibrary) {
        source += "node pure fn identity(value: f64) -> f64 = {\n    value\n}\n";
    } else {
        source += "pub fn on_init() -> i64 = {\n    0\n}\n";
    }

    if (!session_.writeEntry(root + "frate.json", bytes(manifest))
        || !session_.writeEntry(root + "src/lib.fr", bytes(source))) {
        error = "Could not write the pod scaffold into the project VFS.";
        return false;
    }
    return true;
}

bool SuiteFratePodWorkspace::writeSource(const PodScaffoldOptions& options, const juce::String& source,
                                         juce::String& error) {
    const auto name = options.name.trim();
    if (!session_.containsEntry(sourceRootFor(name) + "frate.json") && !scaffold(options, error))
        return false;
    if (!session_.writeEntry(sourceRootFor(name) + "src/lib.fr", bytes(source))) {
        error = "Could not write generated FRust source into the project VFS.";
        return false;
    }
    return true;
}

bool SuiteFratePodWorkspace::materialize(const juce::String& podName, juce::File& outDirectory,
                                         juce::String& error) {
    const auto root = sourceRootFor(podName);
    const auto entries = session_.listEntryPaths();
    juce::StringArray podEntries;
    for (const auto& entry : entries)
        if (entry.startsWith(root)) podEntries.add(entry);
    if (podEntries.isEmpty()) {
        error = "Pod '" + podName + "' does not exist in the project VFS.";
        return false;
    }

    outDirectory = materializationRoot_.getChildFile(session_.getProjectId()).getChildFile(podName);
    if (outDirectory.exists()) outDirectory.deleteRecursively();
    if (!outDirectory.createDirectory()) {
        error = "Could not create the disposable pod build directory.";
        return false;
    }
    for (const auto& entry : podEntries) {
        juce::MemoryBlock data;
        if (!session_.readEntry(entry, data)) {
            error = "Could not read VFS entry " + entry + ".";
            return false;
        }
        const auto relative = entry.substring(root.length());
        const auto target = outDirectory.getChildFile(relative.replaceCharacter('/', juce::File::getSeparatorChar()));
        if (!target.getParentDirectory().createDirectory()
            || !target.replaceWithData(data.getData(), data.getSize())) {
            error = "Could not materialize VFS entry " + entry + ".";
            return false;
        }
    }
    return true;
}

bool SuiteFratePodWorkspace::persistDerived(const juce::String& podName, const juce::File& podDirectory,
                                            juce::String& packageEntry, juce::String& error) {
    const auto root = derivedRootFor(podName);
    const auto buildDirectory = podDirectory.getChildFile("build");
    for (const auto& file : buildDirectory.findChildFiles(juce::File::findFiles, true)) {
        const auto relative = file.getRelativePathFrom(buildDirectory).replaceCharacter('\\', '/');
        if (!session_.writeEntryFromFile(root + "build/" + relative, file, error)) return false;
    }

    const auto package = frate::FratePodBuilder::packagePod(podDirectory);
    if (!package.existsAsFile()) {
        error = "Frate built the pod but could not package it.";
        return false;
    }
    packageEntry = root + package.getFileName();
    return session_.writeEntryFromFile(packageEntry, package, error);
}

PodWorkspaceBuildResult SuiteFratePodWorkspace::build(const juce::String& podName) {
    PodWorkspaceBuildResult result;
    result.sourceRoot = sourceRootFor(podName);
    result.derivedRoot = derivedRootFor(podName);
    juce::File podDirectory;
    juce::String error;
    if (!validPodName(podName) || !materialize(podName, podDirectory, error)) {
        result.build.status = BuildStatus::PodDirectoryInvalid;
        result.build.output = error.isNotEmpty() ? error : "Invalid pod name.";
        return result;
    }
    result.materializedPodDirectory = podDirectory;
    result.build = buildService_.build(podDirectory);
    if (result.build.status != BuildStatus::Success) return result;
    if (!persistDerived(podName, podDirectory, result.packageEntry, error)) {
        result.build.status = BuildStatus::CompileFailed;
        result.build.output = error;
    }
    return result;
}

} // namespace creation::frust
