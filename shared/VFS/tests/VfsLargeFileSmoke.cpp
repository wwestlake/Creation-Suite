// Proves the exFAT switch (VFS-M3) actually lifted the FAT32 4GB per-file
// cap, and that SuiteVolume's read/write chunking handles a file that
// exceeds a single f_write/f_read call's 32-bit UINT byte-count limit.
// Separate from VfsSmoke.cpp so the everyday fast smoke test doesn't pay
// for a multi-GB write/read on every run.
#include <creation/vfs/SuiteVolume.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}

// Deterministic, regeneratable pattern -- lets us verify a multi-GB
// buffer without needing two full copies of it in memory at once.
std::uint8_t patternByteAt(juce::int64 index)
{
    return static_cast<std::uint8_t>(index % 251);
}
}

int main()
{
    try
    {
        const auto containerFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("creation_suite_vfs_largefile_smoke_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".suitevfs");
        containerFile.deleteFile();

        juce::String errorMessage;

        constexpr juce::int64 kVolumeSizeBytes = 6LL * 1024 * 1024 * 1024;   // 6 GiB volume
        constexpr juce::int64 kLargeFileBytes = 4600LL * 1024 * 1024;        // ~4.61 GiB -- past FAT32's 4GiB-1 cap

        {
            creation::vfs::SuiteVolume volume;
            if (! volume.createAndFormat(containerFile, kVolumeSizeBytes, errorMessage))
                fail("createAndFormat failed: " + errorMessage.toStdString());

            {
                juce::MemoryBlock source(static_cast<size_t>(kLargeFileBytes));
                auto* bytes = static_cast<std::uint8_t*>(source.getData());
                for (juce::int64 i = 0; i < kLargeFileBytes; ++i)
                    bytes[i] = patternByteAt(i);

                if (! volume.writeFile("Assets/Source/large-take.bin", source, errorMessage))
                    fail("writeFile of the large file failed: " + errorMessage.toStdString());
            } // source freed before the read-back allocation below

            volume.close();
        }

        {
            creation::vfs::SuiteVolume reopened;
            if (! reopened.open(containerFile, errorMessage))
                fail("Reopening the large-file container failed: " + errorMessage.toStdString());

            juce::MemoryBlock readBack;
            if (! reopened.readFile("Assets/Source/large-take.bin", readBack, errorMessage))
                fail("readFile of the large file failed: " + errorMessage.toStdString());

            if (static_cast<juce::int64>(readBack.getSize()) != kLargeFileBytes)
                fail("Read-back size did not match the written size -- the file was truncated.");

            const auto* bytes = static_cast<const std::uint8_t*>(readBack.getData());
            for (juce::int64 i = 0; i < kLargeFileBytes; ++i)
            {
                if (bytes[i] != patternByteAt(i))
                    fail("Read-back content mismatch at byte offset " + std::to_string(i) + " -- exFAT large-file round trip is broken.");
            }

            reopened.close();
        }

        containerFile.deleteFile();

        std::cout << "VfsLargeFileSmoke: all checks passed (" << (kLargeFileBytes / (1024 * 1024)) << " MB round trip)." << std::endl;
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "VfsLargeFileSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
