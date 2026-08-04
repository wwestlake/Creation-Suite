#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
// HTTP client for reaching the suite VFS service (services/VfsService)
// instead of touching the suite root project's container directly. See
// docs/architecture/Suite-Shared-Project-Model.md, "Mechanism chosen".
//
// Every logical path passed to readEntry/writeEntry/removeEntry is scoped
// under "suite/" by the service itself -- callers just pass their own
// entry name (e.g. "ai-settings.json"), not a full path.
class SuiteVfsServiceClient final
{
public:
    // Finds the running service via SuiteProcessRegistry; if none is
    // found/alive, launches it and polls /health until it responds.
    // Returns false if the service could not be reached within timeoutMs.
    // Safe to call repeatedly -- a no-op once already discovered.
    bool discover(int timeoutMs = 10000);

    bool readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const;
    bool writeEntry(const juce::String& logicalPath, const juce::MemoryBlock& data) const;
    bool removeEntry(const juce::String& logicalPath) const;

private:
    juce::URL entryUrl(const juce::String& logicalPath) const;

    int httpPort_ = 0;
};
}
