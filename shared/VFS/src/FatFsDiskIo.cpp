// Implements FatFs's disk_* interface (diskio.h) against a single plain
// Windows file per drive slot, instead of a real block device. This is the
// entire connection point between FatFs and "our own file-based VFS" -- FatFs
// itself has no idea its "disk" is just a file; it only ever calls these five
// functions.
//
// Drive attach/detach (CreationVfs_AttachDrive/DetachDrive) is our own
// addition, not part of FatFs's public interface -- the higher-level
// SuiteVolume class calls these to bind a real file to a drive number before
// f_mount()/f_mkfs() touch it, and to release it on close.
//
// Uses plain C stdio (_fseeki64/_ftelli64) rather than juce::FileInputStream/
// OutputStream deliberately: this file has no GUI/JUCE dependency at all, and
// 64-bit seeking is required from the start -- even a small FAT volume's
// sector*512 byte offset exceeds a 32-bit long well before any realistic
// project size limit.

#include "ff.h"
#include "diskio.h"
#include "FatFsDiskIo.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <share.h>

namespace {

struct DriveSlot {
    std::FILE* file = nullptr;
    LBA_t sectorCount = 0;
};

std::array<DriveSlot, FF_VOLUMES> g_drives{};
int g_lastAttachErrno = 0;

constexpr WORD kSectorSize = 512;

bool SeekToSector(std::FILE* file, LBA_t sector) {
    const auto offset = static_cast<long long>(sector) * static_cast<long long>(kSectorSize);
    return _fseeki64(file, offset, SEEK_SET) == 0;
}

} // namespace

extern "C" {

// Binds a real file (already created and pre-sized by the caller) to a drive
// slot. sectorCount is the logical volume size in 512-byte sectors -- what
// GET_SECTOR_COUNT reports to f_mkfs()/f_mount(), independent of how much of
// the backing file is a sparse hole versus real allocated disk space.
bool CreationVfs_AttachDrive(BYTE pdrv, const wchar_t* path, LBA_t sectorCount) {
    g_lastAttachErrno = 0;
    if (pdrv >= FF_VOLUMES)
        return false;

    if (g_drives[pdrv].file != nullptr) {
        std::fclose(g_drives[pdrv].file);
        g_drives[pdrv].file = nullptr;
    }

    // _SH_DENYRW: exclusive open. A container is mounted by exactly one
    // SuiteVolume at a time, in exactly one process -- a second open
    // attempt (this process or another) must fail cleanly here rather
    // than silently double-mounting the same backing file, which would
    // corrupt it (two independent FatFs mount states, no shared cache).
    // This is the actual single-owner guarantee VFS-M4's cross-process
    // routing builds on: a sharing-violation failure means someone else
    // already owns this container, right now.
    std::FILE* file = _wfsopen(path, L"r+b", _SH_DENYRW);
    if (file == nullptr) {
        g_lastAttachErrno = errno;
        return false;
    }

    g_drives[pdrv].file = file;
    g_drives[pdrv].sectorCount = sectorCount;
    return true;
}

void CreationVfs_DetachDrive(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES)
        return;

    if (g_drives[pdrv].file != nullptr) {
        std::fclose(g_drives[pdrv].file);
        g_drives[pdrv].file = nullptr;
    }
    g_drives[pdrv].sectorCount = 0;
}

int CreationVfs_LastAttachErrno() {
    return g_lastAttachErrno;
}

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv >= FF_VOLUMES || g_drives[pdrv].file == nullptr)
        return STA_NOINIT;
    return 0;
}

DSTATUS disk_initialize(BYTE pdrv) {
    // The drive is "initialized" the moment CreationVfs_AttachDrive succeeded
    // -- there is no real hardware to spin up, so this is just disk_status.
    return disk_status(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || g_drives[pdrv].file == nullptr)
        return RES_NOTRDY;

    std::FILE* file = g_drives[pdrv].file;
    if (! SeekToSector(file, sector))
        return RES_ERROR;

    const auto got = std::fread(buff, kSectorSize, count, file);
    return got == count ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv >= FF_VOLUMES || g_drives[pdrv].file == nullptr)
        return RES_NOTRDY;

    std::FILE* file = g_drives[pdrv].file;
    if (! SeekToSector(file, sector))
        return RES_ERROR;

    const auto put = std::fwrite(buff, kSectorSize, count, file);
    return put == count ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv >= FF_VOLUMES || g_drives[pdrv].file == nullptr)
        return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            return std::fflush(g_drives[pdrv].file) == 0 ? RES_OK : RES_ERROR;

        case GET_SECTOR_COUNT:
            *static_cast<LBA_t*>(buff) = g_drives[pdrv].sectorCount;
            return RES_OK;

        case GET_SECTOR_SIZE:
            *static_cast<WORD*>(buff) = kSectorSize;
            return RES_OK;

        case GET_BLOCK_SIZE:
            // No meaningful erase-block concept for a plain file backing
            // store (that's an SSD/flash-media concept) -- 1 sector is the
            // correct "no special alignment needed" answer.
            *static_cast<DWORD*>(buff) = 1;
            return RES_OK;

        case CTRL_TRIM:
            // Nothing to trim on a plain file; report success rather than
            // failing an operation FatFs treats as a hint, not a requirement.
            return RES_OK;

        default:
            return RES_PARERR;
    }
}

// FatFs's one other required application-provided hook, for file
// create/modify timestamps -- not implemented anywhere in the vendored
// distribution (get_fattime is always a porting responsibility, same as
// disk_status et al. above). Packed format per FatFs's own documented
// convention: bit31:25 year-1980, 24:21 month, 20:16 day, 15:11 hour,
// 10:5 minute, 4:0 second/2.
DWORD get_fattime(void) {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);

    return (static_cast<DWORD>(local.tm_year + 1900 - 1980) << 25)
         | (static_cast<DWORD>(local.tm_mon + 1) << 21)
         | (static_cast<DWORD>(local.tm_mday) << 16)
         | (static_cast<DWORD>(local.tm_hour) << 11)
         | (static_cast<DWORD>(local.tm_min) << 5)
         | (static_cast<DWORD>(local.tm_sec / 2));
}

} // extern "C"
