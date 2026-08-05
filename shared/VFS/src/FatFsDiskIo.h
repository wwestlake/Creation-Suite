#pragma once

// Internal (not part of the public creation::vfs API) bridge between
// SuiteVolume and the disk_* implementation in FatFsDiskIo.cpp. See that
// file's own header comment for why this exists.

#include "ff.h"

extern "C" {
bool CreationVfs_AttachDrive(BYTE pdrv, const wchar_t* path, LBA_t sectorCount);
void CreationVfs_DetachDrive(BYTE pdrv);
int CreationVfs_LastAttachErrno();
}
