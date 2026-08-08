#include "creation/services/SuiteVfsServiceClient.h"

#include <creation/services/SuiteProcessRegistry.h>

namespace
{
constexpr const char* kServiceAppId = "CreationSuiteVfsService";

// Matches services/VfsService/CMakeLists.txt's post-build copy step --
// every suite executable lands in this shared bin directory.
juce::File findServiceExecutable()
{
#if JUCE_DEBUG
    constexpr const char* kConfigDir = "codex-debug-bin";
#else
    constexpr const char* kConfigDir = "codex-release-bin";
#endif
    return juce::File("D:/CreationSuite-Workspaces")
        .getChildFile(kConfigDir)
        .getChildFile("CreationSuiteVfsService.exe");
}
}

namespace creation::services
{
bool SuiteVfsServiceClient::discover(int timeoutMs)
{
    for (const auto& record : creation::services::SuiteProcessRegistry::EnumerateLiveProcesses())
    {
        if (record.appId == kServiceAppId && record.httpPort > 0)
        {
            httpPort_ = record.httpPort;
            return true;
        }
    }

    const auto serviceExe = findServiceExecutable();
    if (! serviceExe.existsAsFile())
        return false;

    juce::ChildProcess launcher;
    if (! launcher.start(serviceExe.getFullPathName()))
        return false;

    const auto deadline = juce::Time::getCurrentTime() + juce::RelativeTime::milliseconds(timeoutMs);
    while (juce::Time::getCurrentTime() < deadline)
    {
        for (const auto& record : creation::services::SuiteProcessRegistry::EnumerateLiveProcesses())
        {
            if (record.appId == kServiceAppId && record.httpPort > 0)
            {
                httpPort_ = record.httpPort;
                return true;
            }
        }
        juce::Thread::sleep(200);
    }

    return false;
}

juce::URL SuiteVfsServiceClient::entryUrl(const juce::String& logicalPath) const
{
    return juce::URL("http://127.0.0.1:" + juce::String(httpPort_) + "/suite/entry")
        .withParameter("path", logicalPath);
}

bool SuiteVfsServiceClient::readEntry(const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = entryUrl(logicalPath).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
        return false;

    outData.reset();
    stream->readIntoMemoryBlock(outData);
    return true;
}

bool SuiteVfsServiceClient::writeEntry(const juce::String& logicalPath, const juce::MemoryBlock& data) const
{
    if (httpPort_ <= 0)
        return false;

    auto url = entryUrl(logicalPath).withPOSTData(data);

    int statusCode = 0;
    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("PUT")
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    return stream != nullptr && statusCode == 200;
}

bool SuiteVfsServiceClient::removeEntry(const juce::String& logicalPath) const
{
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = entryUrl(logicalPath).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("DELETE")
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    return stream != nullptr && statusCode == 200;
}
}
