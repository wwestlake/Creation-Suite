#pragma once

#include <string>
#include <mutex>

#include <frate/FrateRegistryClient.h>
#include <creation/frust/FratePodVfsResolver.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <juce_core/juce_core.h>

namespace creation::frust {

enum class BuildStatus {
    Success,
    PodDirectoryInvalid,
    DependencyResolutionFailed,
    FrateExecutableNotFound,
    FrateProcessFailedToStart,
    CompileFailed,
};

struct BuildResult {
    BuildStatus status = BuildStatus::FrateExecutableNotFound;
    juce::String output; // frate's own stdout/stderr, for display to the user
    juce::File builtObjectFile; // <podDir>/build/<name>.o, valid only on Success
};

// The other half of the pod pipeline from FratePodVfsResolver: that class
// answers "give me an existing published pod"; this one answers "build
// THIS pod (freshly authored, or schematic-generated) using frate, with
// its dependencies resolved through the Suite VFS cache instead of
// frate's own raw local-file cache" -- the concrete mechanism behind
// "generate code, use frate to build it" and "create a pod in the
// editor, let the user edit it, then build it".
//
// Deliberately does NOT reimplement frate's own dependency-resolution or
// compilation logic (that lives in frate_cli's buildPod(), which isn't
// exposed as a library function) -- runs the real, already-fixed
// `frate build` as a subprocess instead. This is the same boundary a
// human developer crosses running `frate build` from a terminal; the only
// Suite-specific addition is redirecting frate's cache.
//
// Mechanism: frate::FrateCache already honors FRATE_CACHE_DIR as its
// highest-priority cache-root override (see resolveDefaultCacheRoot's own
// doc comment). Before invoking `frate build`, this class materializes
// every dependency the target pod's own frate.json declares into a local
// mirror directory via FratePodVfsResolver (VFS cache -> registry fallback,
// exactly as elsewhere in the Suite), then points FRATE_CACHE_DIR at that
// same mirror so frate's own cache-hit check finds them without ever
// touching the network itself. After the build, anything frate itself
// newly cached there (e.g. a transitive dependency FratePodVfsResolver
// was never asked to pre-materialize) gets swept back into VFS too, so
// the VFS stays the durable, shared record -- not just this one local
// mirror.
class SuiteFrateBuildService {
public:
    SuiteFrateBuildService(creation::services::SuiteVfsServiceClient& vfsClient,
                            frate::FrateRegistryClient& registryClient,
                            juce::File frateExecutable,
                            juce::File localCacheMirror);

    // podDir must be a real, on-disk directory containing a frate.json and
    // its source (exactly what PluginPodWorkspace::createPluginPod already
    // scaffolds into a project's VFS-materialized working copy, or what a
    // schematic codegen pass should write generated source into).
    BuildResult build(const juce::File& podDir);

private:
    static std::mutex processEnvironmentMutex_;
    creation::services::SuiteVfsServiceClient& vfsClient_;
    frate::FrateRegistryClient& registryClient_;
    juce::File frateExecutable_;
    juce::File localCacheMirror_;

    // Returns false only on a real resolution failure (not-found/network/
    // extract error) for a declared dependency -- a dependency already
    // present in localCacheMirror_ is left untouched, not re-resolved.
    bool materializeDependencies(const juce::File& podDir, juce::String& error);

    // Uploads any pod directory under localCacheMirror_ that isn't already
    // a VFS entry -- covers transitive dependencies frate itself fetched
    // during the build, which materializeDependencies() never saw.
    void syncNewCacheEntriesToVfs();
};

} // namespace creation::frust
