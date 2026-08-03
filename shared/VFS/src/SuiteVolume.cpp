#include <creation/vfs/SuiteVolume.h>

#include <array>

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
        errorMessage = "Could not attach the container file to a drive slot.";
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
        errorMessage = "Could not attach the container file to a drive slot.";
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

} // namespace creation::vfs
