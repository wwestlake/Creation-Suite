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
    options.fmt = FM_ANY;
    options.n_fat = 0;   // 0 = FatFs default
    options.align = 0;   // 0 = auto-detect from disk_ioctl(GET_BLOCK_SIZE)
    options.n_root = 0;  // 0 = FatFs default
    options.au_size = 0; // 0 = FatFs default cluster size for the volume size

    std::array<BYTE, 8192> workBuffer{};
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

    UINT written = 0;
    const auto writeResult = f_write(&file, data.getData(), static_cast<UINT>(data.getSize()), &written);
    f_close(&file);

    if (writeResult != FR_OK || written != data.getSize()) {
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

    const auto size = f_size(&file);
    outData.setSize(static_cast<size_t>(size));

    UINT bytesRead = 0;
    const auto readResult = f_read(&file, outData.getData(), static_cast<UINT>(size), &bytesRead);
    f_close(&file);

    if (readResult != FR_OK || bytesRead != size) {
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

} // namespace creation::vfs
