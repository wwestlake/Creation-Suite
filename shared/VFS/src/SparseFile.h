#pragma once

#include <juce_core/juce_core.h>

// Deliberately isolated from FatFsDiskIo.h/ff.h: this file's implementation
// needs <windows.h>, and ff.h's own typedefs (DWORD, WCHAR, BYTE, WORD)
// directly conflict with <windows.h>'s real ones -- they cannot both be
// included in the same translation unit. Keeping this in its own .cpp is
// what makes that possible; SuiteVolume.cpp calls this but never includes
// <windows.h> itself.

namespace creation::vfs {

// Creates containerFile as a new, empty NTFS sparse file whose logical size
// is exactly totalBytes -- Windows only allocates real physical disk blocks
// for byte ranges actually written to later, not for this whole logical size
// up front. Returns false (with containerFile left deleted, not partially
// created) on any failure.
bool CreateSparseContainerFile(const juce::File& containerFile, juce::int64 totalBytes, juce::String& errorMessage);

} // namespace creation::vfs
