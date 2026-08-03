#include <creation/services/SuiteProjectHandoff.h>

#include <cstdint>
#include <cstring>

namespace creation::services {

namespace {

constexpr std::uint32_t kMaxPathBytes = 8192;

// juce::NamedPipe::read/write each wrap a single overlapped ReadFile/
// WriteFile call and return whatever byte count THAT call completed with --
// on a byte-mode pipe that can be fewer than requested, even locally. Every
// transfer in this protocol needs the exact count, so loop until it's all
// moved (or the overall deadline for this call expires).
bool ReadExact(juce::NamedPipe& pipe, void* buffer, int totalBytes, int timeoutMs)
{
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    int received = 0;
    const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds(timeoutMs);
    while (received < totalBytes)
    {
        const auto remaining = deadline - juce::Time::getCurrentTime();
        if (remaining.inMilliseconds() <= 0)
            return false;
        const auto got = pipe.read(bytes + received, totalBytes - received, static_cast<int>(remaining.inMilliseconds()));
        if (got <= 0)
            return false;
        received += got;
    }
    return true;
}

bool WriteExact(juce::NamedPipe& pipe, const void* buffer, int totalBytes, int timeoutMs)
{
    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    int sent = 0;
    const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds(timeoutMs);
    while (sent < totalBytes)
    {
        const auto remaining = deadline - juce::Time::getCurrentTime();
        if (remaining.inMilliseconds() <= 0)
            return false;
        const auto put = pipe.write(bytes + sent, totalBytes - sent, static_cast<int>(remaining.inMilliseconds()));
        if (put <= 0)
            return false;
        sent += put;
    }
    return true;
}

// A just-registered listener's background thread may not have run even
// once yet -- openExisting failing once doesn't mean nobody is listening,
// only "not yet, as of this instant." Retry across the full budget.
bool ConnectWithRetry(juce::NamedPipe& pipe, const juce::String& pipeName, int timeoutMs)
{
    const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds(timeoutMs);
    for (;;)
    {
        if (pipe.openExisting(pipeName))
            return true;
        if (juce::Time::getCurrentTime() >= deadline)
            return false;
        juce::Thread::sleep(10);
    }
}

}

SuiteProjectHandoffListener::SuiteProjectHandoffListener() : juce::Thread("SuiteProjectHandoff") {}

SuiteProjectHandoffListener::~SuiteProjectHandoffListener()
{
    stop();
}

void SuiteProjectHandoffListener::start(const juce::String& pipeName, std::function<void(juce::File)> onReleaseRequested)
{
    pipeName_ = pipeName;
    onReleaseRequested_ = std::move(onReleaseRequested);
    startThread(juce::Thread::Priority::low);
}

void SuiteProjectHandoffListener::stop()
{
    signalThreadShouldExit();
    stopThread(2000);
}

void SuiteProjectHandoffListener::run()
{
    while (! threadShouldExit())
    {
        juce::NamedPipe pipe;
        if (! pipe.createNewPipe(pipeName_))
        {
            wait(50);
            continue;
        }

        // One connect-and-read attempt per pipe instance -- a fresh
        // NamedPipe for each retry rather than repeatedly polling read()
        // on one un-connected instance (PIPE_UNLIMITED_INSTANCES makes a
        // fresh instance cheap, and rapid re-issuing of ConnectNamedPipe
        // on the same handle is exactly the kind of thing worth avoiding).
        std::uint32_t pathLen = 0;
        if (! ReadExact(pipe, &pathLen, sizeof(pathLen), 500) || pathLen == 0 || pathLen > kMaxPathBytes)
            continue;

        juce::MemoryBlock pathBytes(pathLen);
        if (! ReadExact(pipe, pathBytes.getData(), static_cast<int>(pathLen), 2000))
            continue;

        const auto path = juce::String::fromUTF8(static_cast<const char*>(pathBytes.getData()), static_cast<int>(pathLen));

        const std::uint8_t ack = 1;
        WriteExact(pipe, &ack, 1, 2000);

        if (onReleaseRequested_)
            onReleaseRequested_(juce::File(path));
    }
}

bool SuiteProjectHandoffRequestRelease(const juce::String& ownerPipeName, const juce::File& containerFile, int timeoutMs)
{
    juce::NamedPipe pipe;
    if (! ConnectWithRetry(pipe, ownerPipeName, timeoutMs))
        return false;

    const auto* pathUtf8 = containerFile.getFullPathName().toRawUTF8();
    const auto pathLen = static_cast<std::uint32_t>(std::strlen(pathUtf8));
    if (pathLen == 0 || pathLen > kMaxPathBytes)
        return false;

    if (! WriteExact(pipe, &pathLen, sizeof(pathLen), timeoutMs))
        return false;
    if (! WriteExact(pipe, pathUtf8, static_cast<int>(pathLen), timeoutMs))
        return false;

    std::uint8_t ack = 0;
    if (! ReadExact(pipe, &ack, 1, timeoutMs))
        return false;

    return ack == 1;
}

}
