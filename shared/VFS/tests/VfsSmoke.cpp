#include <creation/vfs/SuiteVolume.h>

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

juce::MemoryBlock makeTestBlock(const char* text)
{
    return juce::MemoryBlock(text, std::strlen(text));
}
}

int main()
{
    try
    {
        const auto containerFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("creation_suite_vfs_smoke_" + juce::String(juce::Random::getSystemRandom().nextInt()) + ".suitevfs");
        containerFile.deleteFile();

        juce::String errorMessage;

        // --- create + format ------------------------------------------------
        {
            creation::vfs::SuiteVolume volume;
            constexpr juce::int64 containerSizeBytes = 8 * 1024 * 1024; // 8 MB -- small, fast smoke-test volume
            if (! volume.createAndFormat(containerFile, containerSizeBytes, errorMessage))
                fail("createAndFormat failed: " + errorMessage.toStdString());
            if (! volume.isOpen())
                fail("Volume should report open immediately after createAndFormat.");
            if (! containerFile.existsAsFile())
                fail("createAndFormat should have created a real container file on disk.");

            // Root-level file.
            const auto rootBlock = makeTestBlock("hello from the suite vfs");
            if (! volume.writeFile("hello.txt", rootBlock, errorMessage))
                fail("writeFile(hello.txt) failed: " + errorMessage.toStdString());

            // Nested path -- exercises ensureParentDirectories.
            const auto nestedBlock = makeTestBlock("a fake audio take, as bytes");
            if (! volume.writeFile("Assets/audio/take1.wav", nestedBlock, errorMessage))
                fail("writeFile(Assets/audio/take1.wav) failed: " + errorMessage.toStdString());

            juce::MemoryBlock readBack;
            if (! volume.readFile("hello.txt", readBack, errorMessage))
                fail("readFile(hello.txt) failed: " + errorMessage.toStdString());
            if (readBack != rootBlock)
                fail("readFile(hello.txt) returned different bytes than were written.");

            juce::MemoryBlock nestedReadBack;
            if (! volume.readFile("Assets/audio/take1.wav", nestedReadBack, errorMessage))
                fail("readFile(Assets/audio/take1.wav) failed: " + errorMessage.toStdString());
            if (nestedReadBack != nestedBlock)
                fail("readFile(Assets/audio/take1.wav) returned different bytes than were written.");

            if (! volume.fileExists("hello.txt"))
                fail("fileExists(hello.txt) should be true.");
            if (volume.fileExists("does_not_exist.bin"))
                fail("fileExists(does_not_exist.bin) should be false.");

            // Single-owner guarantee (VFS-M4's foundation): a second open
            // attempt on an already-mounted container must fail cleanly,
            // not silently double-mount and corrupt it.
            creation::vfs::SuiteVolume secondHandle;
            juce::String secondOpenError;
            if (secondHandle.open(containerFile, secondOpenError))
                fail("A second SuiteVolume should not be able to open an already-mounted container.");

            volume.close();
            if (volume.isOpen())
                fail("Volume should report closed after close().");
        }

        // --- reopen: proves persistence across mount/unmount, not just an
        //     in-memory illusion -------------------------------------------
        {
            creation::vfs::SuiteVolume reopened;
            if (! reopened.open(containerFile, errorMessage))
                fail("Reopening the container failed: " + errorMessage.toStdString());

            juce::MemoryBlock readBack;
            if (! reopened.readFile("hello.txt", readBack, errorMessage))
                fail("readFile(hello.txt) after reopen failed: " + errorMessage.toStdString());
            if (readBack != makeTestBlock("hello from the suite vfs"))
                fail("readFile(hello.txt) after reopen returned different bytes.");

            juce::MemoryBlock nestedReadBack;
            if (! reopened.readFile("Assets/audio/take1.wav", nestedReadBack, errorMessage))
                fail("readFile(Assets/audio/take1.wav) after reopen failed: " + errorMessage.toStdString());
            if (nestedReadBack != makeTestBlock("a fake audio take, as bytes"))
                fail("readFile(Assets/audio/take1.wav) after reopen returned different bytes.");

            if (! reopened.deleteFile("hello.txt", errorMessage))
                fail("deleteFile(hello.txt) failed: " + errorMessage.toStdString());
            if (reopened.fileExists("hello.txt"))
                fail("fileExists(hello.txt) should be false after deleteFile.");

            reopened.close();
        }

        containerFile.deleteFile();

        std::cout << "VfsSmoke: all checks passed." << std::endl;
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "VfsSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
