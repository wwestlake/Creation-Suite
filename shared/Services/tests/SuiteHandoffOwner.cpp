// Helper process for SuiteProjectHandoffMultiProcessSmoke, not a test in
// its own right. Registers itself with an "open project," listens for a
// release request, and marks a flag file when its callback actually fires
// -- proving the release really crossed a process boundary rather than
// just exercising the protocol against itself.
#include <creation/services/SuiteProcessRegistry.h>
#include <creation/services/SuiteProjectHandoff.h>

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "usage: SuiteHandoffOwner <appId> <containerPath> <releasedFlagPath>" << std::endl;
        return 2;
    }

    const juce::String appId(argv[1]);
    const juce::File containerFile = juce::String(argv[2]);
    const juce::File releasedFlag = juce::String(argv[3]);
    const juce::File stopFlag = releasedFlag.getSiblingFile(releasedFlag.getFileNameWithoutExtension() + "-stop.flag");

    creation::services::SuiteProcessRegistration registration;
    registration.RegisterSelf(appId);
    registration.SetOpenProject(containerFile);

    creation::services::SuiteProjectHandoffListener listener;
    listener.start(registration.getPipeName(), [&](juce::File requested)
    {
        if (requested == containerFile)
        {
            registration.ClearOpenProject();
            releasedFlag.replaceWithText("released");
        }
    });

    const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::seconds(30);
    while (! stopFlag.existsAsFile() && juce::Time::getCurrentTime() < deadline)
        juce::Thread::sleep(50);

    listener.stop();
    return 0;
}
