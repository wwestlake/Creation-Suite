#include "SparseFile.h"

#include <windows.h>

namespace creation::vfs {

bool CreateSparseContainerFile(const juce::File& containerFile, juce::int64 totalBytes, juce::String& errorMessage) {
    if (! containerFile.getParentDirectory().exists() && ! containerFile.getParentDirectory().createDirectory()) {
        errorMessage = "Could not create the project container's parent directory.";
        return false;
    }

    HANDLE handle = CreateFileW(containerFile.getFullPathName().toWideCharPointer(),
                                GENERIC_READ | GENERIC_WRITE,
                                0, // no sharing -- this handle only exists to allocate the file, never left open
                                nullptr,
                                CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        errorMessage = "Could not create the project container file (it may already exist).";
        return false;
    }

    DWORD bytesReturned = 0;
    const bool sparseOk = DeviceIoControl(handle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &bytesReturned, nullptr) != 0;

    LARGE_INTEGER newSize{};
    newSize.QuadPart = totalBytes;
    const bool seekOk = SetFilePointerEx(handle, newSize, nullptr, FILE_BEGIN) != 0;
    const bool endOk = seekOk && (SetEndOfFile(handle) != 0);

    CloseHandle(handle);

    if (! sparseOk || ! endOk) {
        containerFile.deleteFile();
        errorMessage = "Could not allocate the project container file's logical size.";
        return false;
    }

    return true;
}

} // namespace creation::vfs
