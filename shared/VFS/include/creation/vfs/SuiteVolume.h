#pragma once

#include <memory>

#include <juce_core/juce_core.h>

namespace creation::vfs {

// A single project container: one real file on disk, formatted internally
// as a real FAT/exFAT filesystem via FatFs (see shared/VFS/third_party/fatfs
// and src/FatFsDiskIo.cpp for how that file is made to look like a "disk" to
// FatFs). This is the actual VFS -- callers never see FatFs, sector numbers,
// or the backing file's raw bytes, only logical asset paths.
//
// One process may have up to FF_VOLUMES (see ffconf.h) SuiteVolume instances
// open at once, each bound to its own drive slot internally.
//
// Not yet safe for two separate OS processes to have the same container file
// open at once -- that is VFS-M4's job. Within a single process, a
// SuiteVolume instance is not internally thread-safe; callers must not use
// one instance from multiple threads without their own external locking.
class SuiteVolume final {
public:
    SuiteVolume();
    ~SuiteVolume();

    SuiteVolume(const SuiteVolume&) = delete;
    SuiteVolume& operator=(const SuiteVolume&) = delete;

    // Movable so owning types (ProjectSession) can be returned/relocated
    // without needing a second mount of the same container file.
    SuiteVolume(SuiteVolume&& other) noexcept;
    SuiteVolume& operator=(SuiteVolume&& other) noexcept;

    // Creates a brand-new container file at containerFile (must not already
    // exist) as an NTFS sparse file reporting sizeBytes logically, formats
    // the whole logical size as one fresh FAT volume, then leaves it mounted
    // and ready to use (equivalent to createAndFormat immediately followed
    // by a successful open). sizeBytes is rounded down to a whole number of
    // 512-byte sectors.
    bool createAndFormat(const juce::File& containerFile, juce::int64 sizeBytes, juce::String& errorMessage);

    // Mounts an existing container file previously created by
    // createAndFormat. Fails if this instance is already open, or if no
    // free drive slot is available (see FF_VOLUMES).
    bool open(const juce::File& containerFile, juce::String& errorMessage);

    // Unmounts and releases the drive slot. Safe to call when not open.
    void close();

    bool isOpen() const noexcept { return mounted; }

    // logicalPath is a plain forward-slash path inside the volume, e.g.
    // "Assets/audio/take3.wav" -- intermediate directories are created
    // automatically on write if they don't already exist.
    bool writeFile(const juce::String& logicalPath, const juce::MemoryBlock& data, juce::String& errorMessage);
    bool readFile(const juce::String& logicalPath, juce::MemoryBlock& outData, juce::String& errorMessage) const;
    bool deleteFile(const juce::String& logicalPath, juce::String& errorMessage);
    bool fileExists(const juce::String& logicalPath) const;

    // Recursively lists every file's logical path in the volume (directories
    // are not included). Order is not guaranteed.
    juce::StringArray listFiles() const;

    // ---- What a service needs on top of whole-file read/write. The volume is NOT thread safe (FatFs is built
    // without re-entrancy): the one program that owns it must serialize every call.

    // Size in bytes of a file, or -1 if there is no such file.
    juce::int64 fileSize(const juce::String& logicalPath) const;
    bool directoryExists(const juce::String& logicalPath) const;
    bool createDirectory(const juce::String& logicalPath, juce::String& errorMessage);

    // Reads up to `length` bytes starting at `offset` (fewer at the end of the file). outTotalSize is the whole file's size.
    bool readRange(const juce::String& logicalPath, juce::int64 offset, juce::int64 length,
                   juce::MemoryBlock& outData, juce::int64& outTotalSize, juce::String& errorMessage) const;

    // Writes `size` bytes at `offset`. With truncateFirst the file is created empty first (a new upload's first
    // piece); otherwise the file must already exist and the bytes go at `offset`.
    bool writeAt(const juce::String& logicalPath, juce::int64 offset, const void* data, size_t size,
                 bool truncateFirst, juce::String& errorMessage);

    // Moves a file to a new path, replacing whatever is there.
    bool renameFile(const juce::String& fromPath, const juce::String& toPath, juce::String& errorMessage);
    bool copyFile(const juce::String& fromPath, const juce::String& toPath, juce::String& errorMessage);

    // Deletes a file, or a folder and everything inside it.
    bool deleteTree(const juce::String& logicalPath, juce::String& errorMessage);

    // Names (not paths) of the folders directly inside a folder.
    juce::StringArray listDirectories(const juce::String& logicalPath) const;
    // Every file inside a folder, however deep, as paths relative to that folder.
    juce::StringArray listFilesUnder(const juce::String& logicalPath) const;

private:
    bool acquireDriveSlot(juce::String& errorMessage);
    void releaseDriveSlot();
    bool ensureParentDirectories(const juce::String& logicalPath) const;
    juce::String drivePrefixedPath(const juce::String& logicalPath) const;

    int driveIndex = -1;
    bool mounted = false;
    juce::File openContainerFile;

    // Holds the real FATFS mount-state struct. Kept behind a pimpl rather
    // than a member of type FATFS directly: ff.h #defines BYTE/WORD/DWORD/DIR
    // and others without any namespace, which collide with <windows.h>'s own
    // typedefs (DWORD, BYTE) and POSIX <dirent.h>'s DIR struct -- anything
    // that transitively includes both this header and windows.h (true for
    // essentially every real translation unit in a JUCE app) would fail to
    // compile if ff.h leaked in here. Defined only in SuiteVolume.cpp, which
    // is the one place in the whole suite allowed to include ff.h directly
    // outside of shared/VFS's own src/ files.
    struct MountState;
    std::unique_ptr<MountState> mountState;
};

} // namespace creation::vfs
