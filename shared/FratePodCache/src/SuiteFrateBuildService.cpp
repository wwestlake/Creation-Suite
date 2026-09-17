#include "creation/frust/SuiteFrateBuildService.h"

#include <frate/FrateConfig.h>

namespace creation::frust {

SuiteFrateBuildService::SuiteFrateBuildService(creation::services::SuiteVfsServiceClient& vfsClient,
                                                frate::FrateRegistryClient& registryClient,
                                                juce::File frateExecutable,
                                                juce::File localCacheMirror)
    : vfsClient_(vfsClient), registryClient_(registryClient),
      frateExecutable_(std::move(frateExecutable)), localCacheMirror_(std::move(localCacheMirror)) {}

bool SuiteFrateBuildService::materializeDependencies(const juce::File& podDir, juce::String& error) {
    frate::FrateConfig config;
    if (!config.load(podDir.getChildFile("frate.json"))) {
        error = "Could not read frate.json in " + podDir.getFullPathName();
        return false;
    }

    // Each resolver call is independent -- construct fresh per dependency
    // rather than holding one across the loop, since FratePodVfsResolver
    // has no per-instance state worth reusing.
    for (const auto& dep : config.getMetadata().dependencies) {
        FratePodVfsResolver resolver(vfsClient_, registryClient_, localCacheMirror_);
        juce::File resolvedDir;
        const auto status = resolver.resolve(dep.name, dep.version, resolvedDir);
        if (status != PodResolveStatus::ResolvedFromVfsCache && status != PodResolveStatus::ResolvedFromRegistry) {
            error = "Could not resolve dependency '" + juce::String(dep.name) + "' v" + juce::String(dep.version)
                  + " (status " + juce::String(static_cast<int>(status)) + ")";
            return false;
        }
    }
    return true;
}

void SuiteFrateBuildService::syncNewCacheEntriesToVfs() {
    if (!localCacheMirror_.isDirectory())
        return;

    // <mirror>/<name>/<version>/ -- exactly two levels deep, matching
    // frate::FrateCache::getCachedPodDir. Anything frate itself fetched
    // during the build (a transitive dependency materializeDependencies()
    // never saw, since it only walks the target pod's own direct
    // dependencies) lands here the same way FratePodVfsResolver's own
    // registry-fallback path does, just without going through this
    // process's VFS-write step -- catch up on that now.
    for (const auto& nameDir : localCacheMirror_.findChildFiles(juce::File::findDirectories, false)) {
        for (const auto& versionDir : nameDir.findChildFiles(juce::File::findDirectories, false)) {
            const auto frpodMarker = versionDir.getChildFile("frate.json");
            if (!frpodMarker.existsAsFile())
                continue;

            const auto vfsPath = "frate-cache/" + nameDir.getFileName() + "/" + versionDir.getFileName() + ".frpod";
            juce::MemoryBlock existing;
            if (vfsClient_.readEntry(vfsPath, existing))
                continue; // already the durable record; nothing to do

            // Re-zip the materialized directory back into a .frpod so the
            // VFS entry stays byte-identical in shape to what
            // FratePodVfsResolver itself would have written on a registry
            // download -- same format, whichever path put it there.
            juce::File tempZip = juce::File::createTempFile(".frpod");
            {
                juce::ZipFile::Builder builder;
                for (const auto& file : versionDir.findChildFiles(juce::File::findFiles, true)) {
                    builder.addFile(file, 6, file.getRelativePathFrom(versionDir));
                }
                juce::FileOutputStream out(tempZip);
                if (out.openedOk())
                    builder.writeToStream(out, nullptr);
            }
            juce::MemoryBlock zipBytes;
            if (tempZip.loadFileAsData(zipBytes))
                vfsClient_.writeEntry(vfsPath, zipBytes);
            tempZip.deleteFile();
        }
    }
}

BuildResult SuiteFrateBuildService::build(const juce::File& podDir) {
    BuildResult result;

    if (!podDir.isDirectory() || !podDir.getChildFile("frate.json").existsAsFile()) {
        result.status = BuildStatus::PodDirectoryInvalid;
        result.output = podDir.getFullPathName() + " is not a valid pod directory (no frate.json).";
        return result;
    }

    if (!frateExecutable_.existsAsFile()) {
        result.status = BuildStatus::FrateExecutableNotFound;
        result.output = "frate executable not found at " + frateExecutable_.getFullPathName();
        return result;
    }

    localCacheMirror_.createDirectory();

    juce::String depError;
    if (!materializeDependencies(podDir, depError)) {
        result.status = BuildStatus::DependencyResolutionFailed;
        result.output = depError;
        return result;
    }

    // frate build reads FRATE_CACHE_DIR (highest-priority override,
    // checked before any interactive prompt -- see FrateCache::
    // promptForCacheRootIfUnset) and juce::File::getCurrentWorkingDirectory()
    // (frate.json is always resolved relative to CWD, never passed as an
    // argument). Both need setting before the child process launches.
    _putenv_s("FRATE_CACHE_DIR", localCacheMirror_.getFullPathName().toRawUTF8());

    const auto previousCwd = juce::File::getCurrentWorkingDirectory();
    podDir.setAsCurrentWorkingDirectory();

    juce::ChildProcess process;
    const juce::StringArray args { frateExecutable_.getFullPathName(), "build" };
    const bool started = process.start(args);

    juce::String output;
    int exitCode = -1;
    if (started) {
        output = process.readAllProcessOutput();
        process.waitForProcessToFinish(-1);
        exitCode = process.getExitCode();
    }

    previousCwd.setAsCurrentWorkingDirectory();

    if (!started) {
        result.status = BuildStatus::FrateProcessFailedToStart;
        result.output = "Failed to launch " + frateExecutable_.getFullPathName();
        return result;
    }

    result.output = output;
    if (exitCode != 0) {
        result.status = BuildStatus::CompileFailed;
        return result;
    }

    syncNewCacheEntriesToVfs();

    frate::FrateConfig config;
    if (config.load(podDir.getChildFile("frate.json"))) {
        result.builtObjectFile = podDir.getChildFile("build")
                                      .getChildFile(juce::String(config.getMetadata().name) + ".o");
    }
    result.status = BuildStatus::Success;
    return result;
}

} // namespace creation::frust
