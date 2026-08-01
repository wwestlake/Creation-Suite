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
    // elapse) so a just-started process is discoverable right away.
    void RegisterSelf(const juce::String& appId, int oscPort = 0, const juce::String& pipeName = {});

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
