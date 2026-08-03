// Proves the project handoff protocol works across a real OS process
// boundary: spawns SuiteHandoffOwner as a genuine child process that
// registers an open project, asks it to release over the real IPC path,
// and confirms both the acknowledgement and the actual callback-driven
// release (and the registry update) really happened.
#include <creation/services/SuiteProcessRegistry.h>
#include <creation/services/SuiteProjectHandoff.h>

#include <iostream>
#include <stdexcept>

namespace
{
void fail(const std::string& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message);
}
}

int main()
{
    try
    {
        const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("creation_suite_handoff_smoke_" + juce::String(juce::Random::getSystemRandom().nextInt()));
        tempRoot.deleteRecursively();
        tempRoot.createDirectory();

        // Never actually created as a real VFS container -- this test
        // exercises the handoff protocol and registry, not SuiteVolume.
        const auto containerFile = tempRoot.getChildFile("fake-project.suitevfs");
        const auto releasedFlag = tempRoot.getChildFile("released.flag");
        const auto stopFlag = tempRoot.getChildFile("released-stop.flag");
        const juce::String testAppId = "test-handoff-owner-" + juce::String(juce::Random::getSystemRandom().nextInt());

        const auto ownerExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                                   .getSiblingFile("SuiteHandoffOwner.exe");
        if (! ownerExe.existsAsFile())
            fail("SuiteHandoffOwner.exe not found next to this test executable: " + ownerExe.getFullPathName().toStdString());

        juce::StringArray args;
        args.add(ownerExe.getFullPathName());
        args.add(testAppId);
        args.add(containerFile.getFullPathName());
        args.add(releasedFlag.getFullPathName());

        juce::ChildProcess owner;
        if (! owner.start(args))
            fail("Could not launch SuiteHandoffOwner.");

        creation::services::SuiteProcessRecord ownerRecord;
        bool found = false;
        const auto findDeadline = juce::Time::getCurrentTime() + juce::RelativeTime::seconds(10);
        while (juce::Time::getCurrentTime() < findDeadline)
        {
            if (! owner.isRunning())
                fail("SuiteHandoffOwner exited before registering.");

            for (const auto& record : creation::services::SuiteProcessRegistry::EnumerateLiveProcesses())
            {
                if (record.appId == testAppId && record.openProjectContainerPath == containerFile.getFullPathName())
                {
                    ownerRecord = record;
                    found = true;
                    break;
                }
            }
            if (found)
                break;
            juce::Thread::sleep(25);
        }
        if (! found)
            fail("Never saw the owner's registration with the expected open project.");

        if (! creation::services::SuiteProjectHandoffRequestRelease(ownerRecord.pipeName, containerFile))
            fail("SuiteProjectHandoffRequestRelease did not get an acknowledgement.");

        const auto releaseDeadline = juce::Time::getCurrentTime() + juce::RelativeTime::seconds(5);
        while (! releasedFlag.existsAsFile())
        {
            if (juce::Time::getCurrentTime() > releaseDeadline)
                fail("The owner acknowledged the request but never actually released (no released.flag).");
            juce::Thread::sleep(25);
        }

        bool stillOpen = false;
        for (const auto& record : creation::services::SuiteProcessRegistry::EnumerateLiveProcesses())
            if (record.appId == testAppId && record.openProjectContainerPath == containerFile.getFullPathName())
                stillOpen = true;
        if (stillOpen)
            fail("Registry still shows the project as open after release.");

        stopFlag.replaceWithText("stop");
        if (! owner.waitForProcessToFinish(10000))
            fail("SuiteHandoffOwner did not exit after being told to stop.");
        if (owner.getExitCode() != 0)
            fail("SuiteHandoffOwner exited with a non-zero code.");

        tempRoot.deleteRecursively();

        std::cout << "SuiteProjectHandoffMultiProcessSmoke: all checks passed." << std::endl;
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "SuiteProjectHandoffMultiProcessSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
