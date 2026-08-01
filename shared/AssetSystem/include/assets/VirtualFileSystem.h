#pragma once

#include <creation/assets/VirtualFileSystem.h>

namespace ce::assets
{
// Compatibility wrapper so existing Creation Engine code can ride on the
// suite-level VFS without a disruptive symbol rename pass.
class VirtualFileSystem final
{
public:
    VirtualFileSystem() = default;
    ~VirtualFileSystem() = default;

    VirtualFileSystem(const VirtualFileSystem&) = delete;
    VirtualFileSystem& operator=(const VirtualFileSystem&) = delete;

    bool Mount(const juce::File& zipFile, int priority = 0) { return impl.mount(zipFile, priority); }
    void UnmountAll() { impl.unmountAll(); }

    bool Exists(const juce::String& virtualPath) const { return impl.exists(virtualPath); }
    bool ReadFile(const juce::String& virtualPath, juce::MemoryBlock& outData) const { return impl.readFile(virtualPath, outData); }

    juce::StringArray ListEntries() const { return impl.listEntries(); }

    bool MaterializeEntry(const juce::String& virtualPath,
                          const juce::File& targetDirectory,
                          juce::String& errorMessage,
                          bool overwriteExisting = true) const
    {
        return impl.materializeEntry(virtualPath, targetDirectory, errorMessage, overwriteExisting);
    }

private:
    creation::assets::VirtualFileSystem impl;
};
}
