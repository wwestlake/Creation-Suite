#pragma once

#include <creation/assets/ProjectManifest.h>

#include <juce_core/juce_core.h>
#include <functional>

namespace creation::services
{
// HTTP client for reaching the suite VFS service (services/VfsService) --
// the sole owner of the entire VFS, settings entries and project storage
// alike. See docs/architecture/Suite-Shared-Project-Model.md, "Mechanism
// chosen". No caller ever touches VFS files directly; everything here is
// an HTTP round-trip to the one running service process.
//
// Every logical path passed to readEntry/writeEntry/removeEntry is scoped
// under "suite/" by the service itself -- callers just pass their own
// entry name (e.g. "ai-settings.json"), not a full path.
class SuiteVfsServiceClient final
{
public:
    struct ProjectSummary
    {
        juce::String projectId;
        creation::assets::ProjectManifest manifest;
        std::int64_t totalSizeBytes = 0;
    };

    // Finds the running service via SuiteProcessRegistry; if none is
    // found/alive, launches it and polls /health until it responds.
    // Returns false if the service could not be reached within timeoutMs.
    // Safe to call repeatedly -- a no-op once already discovered.
    bool discover(int timeoutMs = 10000);

    bool readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const;
    bool writeEntry(const juce::String& logicalPath, const juce::MemoryBlock& data) const;
    bool removeEntry(const juce::String& logicalPath) const;

    // Every entry path currently stored (the flat list backing GET /suite/entries) --
    // read-only inspection, not part of the read/write/remove-by-path surface above.
    bool listEntries(juce::StringArray& outPaths) const;

    // Project storage -- every project is a real folder the service owns; callers only ever
    // see it by projectId, never a real filesystem path.
    bool createProject(creation::assets::SuiteAppDomain appDomain, const juce::String& projectName,
                       const juce::String& suiteVersion, const juce::String& appVersion,
                       juce::String& outProjectId, creation::assets::ProjectManifest& outManifest,
                       juce::String& errorMessage) const;
    bool readManifest(const juce::String& projectId, creation::assets::ProjectManifest& outManifest) const;
    bool writeManifest(const juce::String& projectId, const creation::assets::ProjectManifest& manifest) const;
    // Unfiltered by design -- projects are not owned by any app. See
    // docs/architecture/Suite-Shared-Project-Model.md.
    bool listProjects(juce::Array<ProjectSummary>& outProjects) const;
    bool cloneProject(const juce::String& sourceProjectId, const juce::String& newProjectName,
                      juce::String& outNewProjectId, juce::String& errorMessage) const;
    bool deleteProject(const juce::String& projectId, juce::String& errorMessage) const;

    bool readProjectEntry(const juce::String& projectId, const juce::String& logicalPath, juce::MemoryBlock& outData) const;
    bool writeProjectEntry(const juce::String& projectId, const juce::String& logicalPath, const juce::MemoryBlock& data) const;
    // Streams a file up in pieces (16 MB each), so the size is limited by disk, not by one request. `progress`
    // is called after each piece with the fraction done (0..1); returning false cancels the upload and the
    // half-written copy is discarded. Runs on the calling thread - call it from a worker for big files.
    using ProgressFn = std::function<bool(double fraction)>;
    bool writeProjectEntryFromFile(const juce::String& projectId, const juce::String& logicalPath,
                                   const juce::File& sourceFile, const ProgressFn& progress = {}) const;
    // True when the last write failed because `progress` returned false.
    bool lastWriteWasCancelled() const { return lastWriteCancelled_; }
    // For tests that run their own stand-in service.
    void setHttpPortForTesting(int port) { httpPort_ = port; }
    // Why the last writeProjectEntry() failed, in words a user can act on ("" after a success).
    juce::String getLastWriteError() const { return lastWriteError_; }
    bool removeProjectEntry(const juce::String& projectId, const juce::String& logicalPath) const;
    bool listProjectEntries(const juce::String& projectId, juce::StringArray& outPaths) const;

private:
    juce::URL entryUrl(const juce::String& logicalPath) const;
    juce::URL entriesUrl() const;
    juce::URL projectEntryUrl(const juce::String& projectId, const juce::String& logicalPath) const;
    juce::URL baseUrl(const juce::String& path) const;

    int httpPort_ = 0;
    mutable juce::String lastWriteError_;
    mutable bool lastWriteCancelled_ = false;
};
}
