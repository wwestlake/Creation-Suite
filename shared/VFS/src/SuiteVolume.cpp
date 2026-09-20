#include <creation/vfs/SuiteVolume.h>

#include <array>
#include <vector>

#include "ff.h"
#include "FatFsDiskIo.h"
#include "SparseFile.h"

namespace creation::vfs {

struct SuiteVolume::MountState {
    FATFS fatFs{};
};

namespace {

constexpr juce::int64 kSectorSize = 512;

void CollectFiles(const juce::String& drivePrefix, const juce::String& relativePath, juce::StringArray& outPaths) {
    const auto path = relativePath.isEmpty() ? drivePrefix : drivePrefix + relativePath;

    DIR dir{};
    if (f_opendir(&dir, path.toRawUTF8()) != FR_OK)
        return;

    FILINFO info{};
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0) {
        const juce::String name(info.fname);
        const auto childPath = relativePath.isEmpty() ? name : relativePath + "/" + name;
        if (info.fattrib & AM_DIR)
            CollectFiles(drivePrefix, childPath, outPaths);
        else
            outPaths.add(childPath);
    }
    f_closedir(&dir);
}

juce::CriticalSection& DriveSlotLock() {
    static juce::CriticalSection lock;
    return lock;
}

std::array<bool, FF_VOLUMES>& DriveSlotsInUse() {
    static std::array<bool, FF_VOLUMES> slots{};
    return slots;
}

} // namespace

SuiteVolume::SuiteVolume() : mountState(std::make_unique<MountState>()) {}

SuiteVolume::~SuiteVolume() {
    close();
}

SuiteVolume::SuiteVolume(SuiteVolume&& other) noexcept
    : driveIndex(other.driveIndex),
      mounted(other.mounted),
      openContainerFile(std::move(other.openContainerFile)),
      mountState(std::move(other.mountState)) {
    other.driveIndex = -1;
    other.mounted = false;
}

SuiteVolume& SuiteVolume::operator=(SuiteVolume&& other) noexcept {
    if (this != &other) {
        close();
        driveIndex = other.driveIndex;
        mounted = other.mounted;
        openContainerFile = std::move(other.openContainerFile);
        mountState = std::move(other.mountState);
        other.driveIndex = -1;
        other.mounted = false;
    }
    return *this;
}

bool SuiteVolume::acquireDriveSlot(juce::String& errorMessage) {
    const juce::ScopedLock scopedLock(DriveSlotLock());
    auto& slots = DriveSlotsInUse();
    for (int i = 0; i < FF_VOLUMES; ++i) {
        if (! slots[static_cast<size_t>(i)]) {
            slots[static_cast<size_t>(i)] = true;
            driveIndex = i;
            return true;
        }
    }
    errorMessage = "No free suite VFS drive slot available (FF_VOLUMES exhausted).";
    return false;
}

void SuiteVolume::releaseDriveSlot() {
    if (driveIndex < 0)
        return;
    const juce::ScopedLock scopedLock(DriveSlotLock());
    DriveSlotsInUse()[static_cast<size_t>(driveIndex)] = false;
    driveIndex = -1;
}

juce::String SuiteVolume::drivePrefixedPath(const juce::String& logicalPath) const {
    auto cleaned = logicalPath.replaceCharacter('\\', '/');
    while (cleaned.startsWithChar('/'))
        cleaned = cleaned.substring(1);
    return juce::String(driveIndex) + ":/" + cleaned;
}

bool SuiteVolume::ensureParentDirectories(const juce::String& logicalPath) const {
    const auto lastSlash = logicalPath.lastIndexOfChar('/');
    if (lastSlash <= 0)
        return true;

    const auto directoryPart = logicalPath.substring(0, lastSlash);
    juce::StringArray segments;
    segments.addTokens(directoryPart, "/", "");

    juce::String built;
    for (const auto& segment : segments) {
        if (segment.isEmpty())
            continue;
        built += (built.isEmpty() ? segment : ("/" + segment));
        const auto fr = f_mkdir(drivePrefixedPath(built).toRawUTF8());
        if (fr != FR_OK && fr != FR_EXIST)
            return false;
    }
    return true;
}

bool SuiteVolume::createAndFormat(const juce::File& containerFile, juce::int64 sizeBytes, juce::String& errorMessage) {
    if (mounted) {
        errorMessage = "This SuiteVolume is already open.";
        return false;
    }
    if (containerFile.existsAsFile()) {
        errorMessage = "The target container file already exists.";
        return false;
    }

    const auto sectorCount = static_cast<LBA_t>(sizeBytes / kSectorSize);
    if (sectorCount == 0) {
        errorMessage = "Requested container size is smaller than one sector.";
        return false;
    }

    if (! CreateSparseContainerFile(containerFile, static_cast<juce::int64>(sectorCount) * kSectorSize, errorMessage))
        return false;

    if (! acquireDriveSlot(errorMessage)) {
        containerFile.deleteFile();
        return false;
    }

    if (! CreationVfs_AttachDrive(static_cast<BYTE>(driveIndex), containerFile.getFullPathName().toWideCharPointer(), sectorCount)) {
        errorMessage = "Could not attach the container file to a drive slot (errno "
                     + juce::String(CreationVfs_LastAttachErrno()) + ").";
        releaseDriveSlot();
        containerFile.deleteFile();
        return false;
    }

    const auto drivePrefix = juce::String(driveIndex) + ":";

    MKFS_PARM options{};
    options.fmt = FM_EXFAT; // exFAT specifically: FAT32's 4GB per-file cap is unacceptable for real project assets (video, audio).
    options.n_fat = 0;   // 0 = FatFs default
    options.align = 0;   // 0 = auto-detect from disk_ioctl(GET_BLOCK_SIZE)
    options.n_root = 0;  // 0 = FatFs default
    options.au_size = 0; // 0 = FatFs default cluster size for the volume size

    std::array<BYTE, 32768> workBuffer{};
    const auto mkfsResult = f_mkfs(drivePrefix.toRawUTF8(), &options, workBuffer.data(), static_cast<UINT>(workBuffer.size()));
    if (mkfsResult != FR_OK) {
        errorMessage = "Could not format the project container (FatFs error " + juce::String(static_cast<int>(mkfsResult)) + ").";
        CreationVfs_DetachDrive(static_cast<BYTE>(driveIndex));
        releaseDriveSlot();
        containerFile.deleteFile();
        return false;
    }

    const auto mountResult = f_mount(&mountState->fatFs, drivePrefix.toRawUTF8(), 1);
    if (mountResult != FR_OK) {
        errorMessage = "Formatted the container but could not mount it (FatFs error " + juce::String(static_cast<int>(mountResult)) + ").";
        CreationVfs_DetachDrive(static_cast<BYTE>(driveIndex));
        releaseDriveSlot();
        containerFile.deleteFile();
        return false;
    }

    mounted = true;
    openContainerFile = containerFile;
    return true;
}

bool SuiteVolume::open(const juce::File& containerFile, juce::String& errorMessage) {
    if (mounted) {
        errorMessage = "This SuiteVolume is already open.";
        return false;
    }
    if (! containerFile.existsAsFile()) {
        errorMessage = "The project container file does not exist.";
        return false;
    }

    const auto sectorCount = static_cast<LBA_t>(containerFile.getSize() / kSectorSize);

    if (! acquireDriveSlot(errorMessage))
        return false;

    if (! CreationVfs_AttachDrive(static_cast<BYTE>(driveIndex), containerFile.getFullPathName().toWideCharPointer(), sectorCount)) {
        errorMessage = "Could not attach the container file to a drive slot (errno "
                     + juce::String(CreationVfs_LastAttachErrno()) + ").";
        releaseDriveSlot();
        return false;
    }

    const auto drivePrefix = juce::String(driveIndex) + ":";
    const auto mountResult = f_mount(&mountState->fatFs, drivePrefix.toRawUTF8(), 1);
    if (mountResult != FR_OK) {
        errorMessage = "Could not mount the project container (FatFs error " + juce::String(static_cast<int>(mountResult)) + ").";
        CreationVfs_DetachDrive(static_cast<BYTE>(driveIndex));
        releaseDriveSlot();
        return false;
    }

    mounted = true;
    openContainerFile = containerFile;
    return true;
}

void SuiteVolume::close() {
    if (! mounted)
        return;

    const auto drivePrefix = juce::String(driveIndex) + ":";
    f_mount(nullptr, drivePrefix.toRawUTF8(), 0);
    CreationVfs_DetachDrive(static_cast<BYTE>(driveIndex));
    releaseDriveSlot();
    mounted = false;
    openContainerFile = juce::File();
}

namespace {
// f_write/f_read take a 32-bit UINT byte count, so a single call cannot move
// more than ~4GB regardless of the underlying FAT format -- files larger
// than that (the whole point of enabling exFAT) must be moved in bounded
// chunks. 32MB balances call overhead against not needing a second large
// buffer.
constexpr size_t kIoChunkBytes = 32 * 1024 * 1024;
}

bool SuiteVolume::writeFile(const juce::String& logicalPath, const juce::MemoryBlock& data, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }
    if (! ensureParentDirectories(logicalPath)) {
        errorMessage = "Could not create the asset's parent directories.";
        return false;
    }

    FIL file{};
    const auto openResult = f_open(&file, drivePrefixedPath(logicalPath).toRawUTF8(), FA_CREATE_ALWAYS | FA_WRITE);
    if (openResult != FR_OK) {
        errorMessage = "Could not create the asset entry (FatFs error " + juce::String(static_cast<int>(openResult)) + ").";
        return false;
    }

    const auto* bytes = static_cast<const std::uint8_t*>(data.getData());
    size_t remaining = data.getSize();
    size_t offset = 0;
    bool ok = true;

    while (remaining > 0) {
        const auto thisChunk = static_cast<UINT>(juce::jmin(remaining, kIoChunkBytes));
        UINT written = 0;
        const auto writeResult = f_write(&file, bytes + offset, thisChunk, &written);
        if (writeResult != FR_OK || written != thisChunk) {
            ok = false;
            break;
        }
        offset += thisChunk;
        remaining -= thisChunk;
    }

    f_close(&file);

    if (! ok) {
        errorMessage = "Could not write the asset's data.";
        return false;
    }
    return true;
}

bool SuiteVolume::readFile(const juce::String& logicalPath, juce::MemoryBlock& outData, juce::String& errorMessage) const {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }

    FIL file{};
    const auto openResult = f_open(&file, drivePrefixedPath(logicalPath).toRawUTF8(), FA_READ);
    if (openResult != FR_OK) {
        errorMessage = "The requested asset was not found (FatFs error " + juce::String(static_cast<int>(openResult)) + ").";
        return false;
    }

    const FSIZE_t size = f_size(&file);
    outData.setSize(static_cast<size_t>(size));

    auto* bytes = static_cast<std::uint8_t*>(outData.getData());
    FSIZE_t remaining = size;
    size_t offset = 0;
    bool ok = true;

    while (remaining > 0) {
        const auto thisChunk = static_cast<UINT>(juce::jmin<FSIZE_t>(remaining, static_cast<FSIZE_t>(kIoChunkBytes)));
        UINT bytesRead = 0;
        const auto readResult = f_read(&file, bytes + offset, thisChunk, &bytesRead);
        if (readResult != FR_OK || bytesRead != thisChunk) {
            ok = false;
            break;
        }
        offset += thisChunk;
        remaining -= thisChunk;
    }

    f_close(&file);

    if (! ok) {
        errorMessage = "Could not read the asset's data.";
        return false;
    }
    return true;
}

bool SuiteVolume::deleteFile(const juce::String& logicalPath, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }
    const auto result = f_unlink(drivePrefixedPath(logicalPath).toRawUTF8());
    if (result != FR_OK) {
        errorMessage = "Could not delete the asset (FatFs error " + juce::String(static_cast<int>(result)) + ").";
        return false;
    }
    return true;
}

bool SuiteVolume::fileExists(const juce::String& logicalPath) const {
    if (! mounted)
        return false;
    FILINFO info{};
    return f_stat(drivePrefixedPath(logicalPath).toRawUTF8(), &info) == FR_OK;
}

juce::StringArray SuiteVolume::listFiles() const {
    juce::StringArray results;
    if (! mounted)
        return results;

    CollectFiles(juce::String(driveIndex) + ":/", {}, results);
    return results;
}

juce::int64 SuiteVolume::fileSize(const juce::String& logicalPath) const {
    if (! mounted)
        return -1;
    FILINFO info{};
    if (f_stat(drivePrefixedPath(logicalPath).toRawUTF8(), &info) != FR_OK || (info.fattrib & AM_DIR))
        return -1;
    return static_cast<juce::int64>(info.fsize);
}

bool SuiteVolume::directoryExists(const juce::String& logicalPath) const {
    if (! mounted)
        return false;
    FILINFO info{};
    return f_stat(drivePrefixedPath(logicalPath).toRawUTF8(), &info) == FR_OK && (info.fattrib & AM_DIR) != 0;
}

bool SuiteVolume::createDirectory(const juce::String& logicalPath, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }
    // ensureParentDirectories makes every folder up to (not including) the last segment; a trailing "/" makes the
    // last segment a parent too.
    if (! ensureParentDirectories(logicalPath + "/x")) {
        errorMessage = "Could not create the folder.";
        return false;
    }
    return true;
}

bool SuiteVolume::readRange(const juce::String& logicalPath, juce::int64 offset, juce::int64 length,
                            juce::MemoryBlock& outData, juce::int64& outTotalSize, juce::String& errorMessage) const {
    outData.reset();
    outTotalSize = 0;
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }

    FIL file{};
    if (f_open(&file, drivePrefixedPath(logicalPath).toRawUTF8(), FA_READ) != FR_OK) {
        errorMessage = "The requested asset was not found.";
        return false;
    }

    const auto total = static_cast<juce::int64>(f_size(&file));
    outTotalSize = total;
    if (offset < 0 || offset > total || length < 0) {
        f_close(&file);
        errorMessage = "The requested range is outside the asset.";
        return false;
    }

    const auto count = juce::jmin(length, total - offset);
    if (count == 0) {
        f_close(&file);
        return true;
    }

    outData.setSize(static_cast<size_t>(count));
    bool ok = f_lseek(&file, static_cast<FSIZE_t>(offset)) == FR_OK;
    auto* bytes = static_cast<std::uint8_t*>(outData.getData());
    size_t done = 0;
    while (ok && done < static_cast<size_t>(count)) {
        const auto thisChunk = static_cast<UINT>(juce::jmin(static_cast<size_t>(count) - done, kIoChunkBytes));
        UINT bytesRead = 0;
        if (f_read(&file, bytes + done, thisChunk, &bytesRead) != FR_OK || bytesRead != thisChunk)
            ok = false;
        done += thisChunk;
    }

    f_close(&file);
    if (! ok) {
        errorMessage = "Could not read the asset's data.";
        outData.reset();
        return false;
    }
    return true;
}

bool SuiteVolume::writeAt(const juce::String& logicalPath, juce::int64 offset, const void* data, size_t size,
                          bool truncateFirst, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }
    if (offset < 0) {
        errorMessage = "A write cannot start before the beginning of a file.";
        return false;
    }
    if (truncateFirst && ! ensureParentDirectories(logicalPath)) {
        errorMessage = "Could not create the asset's parent directories.";
        return false;
    }

    FIL file{};
    const BYTE mode = truncateFirst ? static_cast<BYTE>(FA_CREATE_ALWAYS | FA_WRITE) : static_cast<BYTE>(FA_OPEN_EXISTING | FA_WRITE);
    if (f_open(&file, drivePrefixedPath(logicalPath).toRawUTF8(), mode) != FR_OK) {
        errorMessage = "Could not open the asset for writing.";
        return false;
    }

    bool ok = offset == 0 || f_lseek(&file, static_cast<FSIZE_t>(offset)) == FR_OK;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    size_t done = 0;
    while (ok && done < size) {
        const auto thisChunk = static_cast<UINT>(juce::jmin(size - done, kIoChunkBytes));
        UINT written = 0;
        if (f_write(&file, bytes + done, thisChunk, &written) != FR_OK || written != thisChunk)
            ok = false;
        done += thisChunk;
    }

    f_close(&file);
    if (! ok) {
        errorMessage = "Could not write the asset's data (is the container full?).";
        return false;
    }
    return true;
}

bool SuiteVolume::renameFile(const juce::String& fromPath, const juce::String& toPath, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }
    if (! ensureParentDirectories(toPath)) {
        errorMessage = "Could not create the destination's parent directories.";
        return false;
    }

    auto target = toPath.replaceCharacter('\\', '/');
    while (target.startsWithChar('/'))
        target = target.substring(1);

    f_unlink(drivePrefixedPath(toPath).toRawUTF8()); // replace whatever is there; failing because nothing is there is fine
    // FatFs takes the new name without a drive number: it stays on the old name's volume.
    if (f_rename(drivePrefixedPath(fromPath).toRawUTF8(), target.toRawUTF8()) != FR_OK) {
        errorMessage = "Could not rename the asset.";
        return false;
    }
    return true;
}

bool SuiteVolume::copyFile(const juce::String& fromPath, const juce::String& toPath, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }
    if (! ensureParentDirectories(toPath)) {
        errorMessage = "Could not create the destination's parent directories.";
        return false;
    }

    FIL source{};
    if (f_open(&source, drivePrefixedPath(fromPath).toRawUTF8(), FA_READ) != FR_OK) {
        errorMessage = "The asset to copy was not found.";
        return false;
    }

    FIL destination{};
    if (f_open(&destination, drivePrefixedPath(toPath).toRawUTF8(), FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        f_close(&source);
        errorMessage = "Could not create the copy.";
        return false;
    }

    constexpr size_t kCopyBytes = 4 * 1024 * 1024;
    std::vector<std::uint8_t> buffer(kCopyBytes);
    bool ok = true;
    FSIZE_t remaining = f_size(&source);
    while (ok && remaining > 0) {
        const auto thisChunk = static_cast<UINT>(juce::jmin<FSIZE_t>(remaining, static_cast<FSIZE_t>(kCopyBytes)));
        UINT bytesRead = 0, bytesWritten = 0;
        if (f_read(&source, buffer.data(), thisChunk, &bytesRead) != FR_OK || bytesRead != thisChunk
            || f_write(&destination, buffer.data(), bytesRead, &bytesWritten) != FR_OK || bytesWritten != bytesRead)
            ok = false;
        remaining -= thisChunk;
    }

    f_close(&destination);
    f_close(&source);
    if (! ok) {
        errorMessage = "Could not copy the asset's data.";
        return false;
    }
    return true;
}

namespace {
bool DeleteTreeAt(const juce::String& drivePrefix, const juce::String& relativePath) {
    const auto full = drivePrefix + relativePath;
    FILINFO info{};
    if (f_stat(full.toRawUTF8(), &info) != FR_OK)
        return false;

    if (! (info.fattrib & AM_DIR))
        return f_unlink(full.toRawUTF8()) == FR_OK;

    // Collect the children first: FatFs does not like a directory being changed while it is being read.
    juce::StringArray children;
    DIR dir{};
    if (f_opendir(&dir, full.toRawUTF8()) != FR_OK)
        return false;
    FILINFO child{};
    while (f_readdir(&dir, &child) == FR_OK && child.fname[0] != 0)
        children.add(juce::String(child.fname));
    f_closedir(&dir);

    for (const auto& name : children)
        if (! DeleteTreeAt(drivePrefix, relativePath + "/" + name))
            return false;

    return f_unlink(full.toRawUTF8()) == FR_OK;
}
} // namespace

bool SuiteVolume::deleteTree(const juce::String& logicalPath, juce::String& errorMessage) {
    if (! mounted) {
        errorMessage = "The volume is not open.";
        return false;
    }

    auto cleaned = logicalPath.replaceCharacter('\\', '/');
    while (cleaned.startsWithChar('/'))
        cleaned = cleaned.substring(1);
    while (cleaned.endsWithChar('/'))
        cleaned = cleaned.dropLastCharacters(1);
    if (cleaned.isEmpty()) {
        errorMessage = "Refusing to delete the whole volume.";
        return false;
    }

    if (! DeleteTreeAt(juce::String(driveIndex) + ":/", cleaned)) {
        errorMessage = "Could not delete it.";
        return false;
    }
    return true;
}

juce::StringArray SuiteVolume::listDirectories(const juce::String& logicalPath) const {
    juce::StringArray names;
    if (! mounted)
        return names;

    auto cleaned = logicalPath.replaceCharacter('\\', '/');
    while (cleaned.startsWithChar('/'))
        cleaned = cleaned.substring(1);

    DIR dir{};
    if (f_opendir(&dir, (juce::String(driveIndex) + ":/" + cleaned).toRawUTF8()) != FR_OK)
        return names;

    FILINFO info{};
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != 0)
        if (info.fattrib & AM_DIR)
            names.add(juce::String(info.fname));
    f_closedir(&dir);
    return names;
}

juce::StringArray SuiteVolume::listFilesUnder(const juce::String& logicalPath) const {
    juce::StringArray results;
    if (! mounted)
        return results;

    auto cleaned = logicalPath.replaceCharacter('\\', '/');
    while (cleaned.startsWithChar('/'))
        cleaned = cleaned.substring(1);
    while (cleaned.endsWithChar('/'))
        cleaned = cleaned.dropLastCharacters(1);

    juce::StringArray fromVolumeRoot;
    CollectFiles(juce::String(driveIndex) + ":/", cleaned, fromVolumeRoot);

    const auto prefix = cleaned.isEmpty() ? juce::String() : cleaned + "/";
    for (const auto& path : fromVolumeRoot)
        results.add(path.startsWith(prefix) ? path.substring(prefix.length()) : path);
    return results;
}

} // namespace creation::vfs
