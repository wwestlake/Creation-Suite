#pragma once

#include <functional>

#include <juce_core/juce_core.h>

namespace creation::services {

// Owner-side: listens on this process's own control pipe (see
// SuiteProcessRegistration::getPipeName()) for release requests from other
// suite apps and, on receipt, calls onReleaseRequested with the requested
// container path. This is single-creator coordination -- the same
// person's apps handing a project back and forth -- not multi-user
// locking: the owner is expected to release cooperatively and promptly,
// and since projects are saved continuously, releasing is just closing
// the mount, nothing to flush first.
//
// onReleaseRequested is called on this listener's own background thread,
// not the message thread -- wrap it in juce::MessageManager::callAsync
// yourself if your callback touches UI state. Kept this way so this class
// (like SuiteVolume) only needs juce_core, not juce_events.
class SuiteProjectHandoffListener final : private juce::Thread {
public:
    SuiteProjectHandoffListener();
    ~SuiteProjectHandoffListener() override;

    void start(const juce::String& pipeName, std::function<void(juce::File)> onReleaseRequested);
    void stop();

private:
    void run() override;

    juce::String pipeName_;
    std::function<void(juce::File)> onReleaseRequested_;
};

// Requester-side: asks the process listening on ownerPipeName to release
// containerFile, waiting up to timeoutMs for an acknowledgement. Returns
// false if the request could not be delivered/acknowledged in time --
// callers should treat that as "the ask didn't land," not as proof the
// container is still locked, and fall back to polling SuiteVolume::open()
// regardless. The request is an optimization that hurries a release
// along; it is not the only way the release can happen.
bool SuiteProjectHandoffRequestRelease(const juce::String& ownerPipeName, const juce::File& containerFile, int timeoutMs = 3000);

}
