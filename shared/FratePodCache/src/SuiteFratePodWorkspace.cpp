#include "creation/frust/SuiteFratePodWorkspace.h"


namespace creation::frust {
namespace {

juce::MemoryBlock bytes(const juce::String& text) {
    return { text.toRawUTF8(), static_cast<size_t>(text.getNumBytesAsUTF8()) };
}

juce::String jsonString(const juce::String& value) {
    return juce::JSON::toString(juce::var(value), false);
}

} // namespace

SuitePodSource::SuitePodSource(creation::assets::ProjectSession& session, FratePodVfsResolver& resolver)
    : session_(session), resolver_(resolver) {}

bool SuitePodSource::findPod(const std::string& name, const std::string& version, frate::PodFiles& files) {
    // A pod authored in this project wins, if it is the version asked for.
    if (session_.isValid()) {
        SuiteFratePodWorkspace local(session_, *this);
        frate::PodFiles own;
        juce::String ignored;
        if (local.readPod(juce::String(name), own, ignored)) {
            const auto manifest = own.find("frate.json");
            if (manifest != own.end()) {
                const juce::var parsed = juce::JSON::parse(juce::String(manifest->second));
                if (parsed.getProperty("version", {}).toString().toStdString() == version) {
                    files = std::move(own);
                    return true;
                }
            }
        }
    }

    const auto status = resolver_.resolve(name, version, files);
    return status == PodResolveStatus::ResolvedFromVfsCache || status == PodResolveStatus::ResolvedFromRegistry;
}

SuiteFratePodWorkspace::SuiteFratePodWorkspace(creation::assets::ProjectSession& session,
                                               frate::PodSource& dependencies)
    : session_(session), dependencies_(dependencies) {}

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

bool SuiteFratePodWorkspace::readPod(const juce::String& podName, frate::PodFiles& files,
                                      juce::String& error) const {
    const auto root = sourceRootFor(podName);
    const auto entries = session_.listEntryPaths();
    frate::PodFiles result;
    for (const auto& entry : entries) {
        if (!entry.startsWith(root)) continue;
        juce::MemoryBlock data;
        if (!session_.readEntry(entry, data)) {
            error = "Could not read VFS entry " + entry + ".";
            return false;
        }
        result[entry.substring(root.length()).toStdString()]
            .assign(static_cast<const char*>(data.getData()), data.getSize());
    }
    if (result.empty()) {
        error = "Pod " + podName + " does not exist in the project VFS.";
        return false;
    }
    files = std::move(result);
    return true;
}

PodWorkspaceBuildResult SuiteFratePodWorkspace::build(const juce::String& podName) {
    PodWorkspaceBuildResult result;
    result.sourceRoot = sourceRootFor(podName);
    result.derivedRoot = derivedRootFor(podName);

    juce::String error;
    if (!validPodName(podName) || !readPod(podName, result.files, error)) {
        result.output = error.isNotEmpty() ? error : "Invalid pod name.";
        return result;
    }

    const auto built = frate::buildPod(result.files, &dependencies_);
    if (!built.ok) {
        for (const auto& diagnostic : built.diagnostics)
            result.output << juce::String(::frust::FormatDiagnostic(diagnostic)) << "\n";
        if (result.output.isEmpty()) result.output = "The FRust compiler reported a failure without a message.";
        return result;
    }

    // Build output goes back into the project VFS as entries, straight from memory.
    result.objectEntry = result.derivedRoot + "build/" + juce::String(built.name) + ".o";
    const juce::MemoryBlock object(built.object.data(), built.object.size());
    std::string packageBytes, packError;
    if (!frate::packPod(result.files, packageBytes, packError)) {
        result.output = "The pod built but could not be packaged: " + juce::String(packError);
        return result;
    }
    result.packageEntry = result.derivedRoot + juce::String(built.name) + "-" + juce::String(built.version) + ".frpod";
    if (!session_.writeEntry(result.objectEntry, object)
        || !session_.writeEntry(result.packageEntry, juce::MemoryBlock(packageBytes.data(), packageBytes.size()))) {
        result.output = "The pod built but its output could not be written into the project VFS.";
        return result;
    }

    result.success = true;
    result.output = "Built " + juce::String(built.name) + " " + juce::String(built.version) + ".";
    return result;
}

} // namespace creation::frust
