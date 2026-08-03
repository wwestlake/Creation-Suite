#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
// One running suite app's discovery record -- what another process
// needs to find and reach it. oscPort == 0 / pipeName empty means "not
// offering that endpoint" (a process can register purely to be
// discoverable without exposing either IPC-2 or IPC-3 transport itself).
struct SuiteProcessRecord
{
    juce::String appId;
    juce::uint32 processId = 0;
    int oscPort = 0;
    juce::String pipeName;
    juce::Time startedAt;
    juce::Time lastHeartbeat;

    // Full path of the project container this process currently has open,
    // or empty if none. This is how another app finds out who to ask for
    // a project before it bothers trying (and failing) to mount it --
    // see SuiteProjectHandoff.h for the actual release request.
    juce::String openProjectContainerPath;
};

// Owns this process's OWN registration file and the background thread
// that keeps it fresh. One instance per running app, held for the
// app's entire lifetime -- construct it, call RegisterSelf() once at
// startup, let it live until the app closes. Never touches any
// real-time thread: all file I/O happens on this class's own
// juce::Thread, which simply sleeps between heartbeats.
//
// A crash (no graceful destructor run) leaves a stale registration file
// behind on purpose rather than trying to detect crashes -- that's
// exactly what SuiteProcessRegistry::EnumerateLiveProcesses' staleness
// filter is for, not something this class needs to handle itself.
class SuiteProcessRegistration final : private juce::Thread
{
public:
    SuiteProcessRegistration();
    ~SuiteProcessRegistration() override;

    // Starts the heartbeat thread and writes the initial registration
    // file immediately (not waiting for the first heartbeat interval to
    // elapse) so a just-started process is discoverable right away. If
    // pipeName is left empty, a real one is generated (unique per app +
    // process) so this process is always reachable for control-channel
    // requests -- see SuiteProjectHandoff.h -- without every caller
    // needing to invent its own naming scheme.
    void RegisterSelf(const juce::String& appId, int oscPort = 0, const juce::String& pipeName = {});

    // The pipe name this process is actually listening on (whatever was
    // passed to RegisterSelf, or the generated one). Wire a
    // SuiteProjectHandoffListener to this same name so requesters --
    // which read it back out of the registry -- can actually reach it.
    juce::String getPipeName() const noexcept { return pipeName_; }

    // Publishes/clears which project container this process currently has
    // open, so another process can find out who owns a project before
    // trying (and failing) to mount it itself. Rewrites the heartbeat file
    // immediately rather than waiting for the next interval, since a
    // project open/close is exactly the moment another app might be
    // checking.
    void SetOpenProject(const juce::File& containerFile);
    void ClearOpenProject();

    static constexpr int kHeartbeatIntervalMs = 5000;

private:
    void run() override;
    void WriteHeartbeatFile();
    juce::File RegistrationFile() const;

    juce::String appId_;
    int oscPort_ = 0;
    juce::String pipeName_;
    juce::Time startedAt_;
    juce::uint32 processId_ = 0;
    juce::String openProjectContainerPath_;
};

// Discovery side -- stateless, callable by any process (whether or not
// it's registered itself) to find its live suite-mates.
class SuiteProcessRegistry final
{
public:
    // Records with no heartbeat within staleSeconds are excluded from
    // the result AND their file is deleted -- a crashed process's stale
    // entry doesn't linger cluttering the registry directory forever.
    // Default of 15s is 3x the heartbeat interval, a standard margin
    // against one missed beat before declaring a process dead.
    static juce::Array<SuiteProcessRecord> EnumerateLiveProcesses(double staleSeconds = 15.0);

    static juce::File RegistryDirectory();
};
}
