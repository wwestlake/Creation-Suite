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

juce::URL SuiteVfsServiceClient::entriesUrl() const
{
    return juce::URL("http://127.0.0.1:" + juce::String(httpPort_) + "/suite/entries");
}

juce::URL SuiteVfsServiceClient::baseUrl(const juce::String& path) const
{
    return juce::URL("http://127.0.0.1:" + juce::String(httpPort_) + path);
}

juce::URL SuiteVfsServiceClient::projectEntryUrl(const juce::String& projectId, const juce::String& logicalPath) const
{
    return baseUrl("/project/entry").withParameter("projectId", projectId).withParameter("path", logicalPath);
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
            .withExtraHeaders("Content-Type: application/octet-stream\r\n")
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

bool SuiteVfsServiceClient::listEntries(juce::StringArray& outPaths) const
{
    outPaths.clear();
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = entriesUrl().createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
        return false;

    const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
    const auto* array = parsed.getArray();
    if (array == nullptr)
        return false;

    for (const auto& entry : *array)
        outPaths.add(entry.toString());

    return true;
}

bool SuiteVfsServiceClient::createProject(creation::assets::SuiteAppDomain appDomain, const juce::String& projectName,
                                          const juce::String& suiteVersion, const juce::String& appVersion,
                                          juce::String& outProjectId, creation::assets::ProjectManifest& outManifest,
                                          juce::String& errorMessage) const
{
    if (httpPort_ <= 0)
    {
        errorMessage = "The suite VFS service has not been discovered yet.";
        return false;
    }

    auto* body = new juce::DynamicObject();
    body->setProperty("appDomain", creation::assets::toStorageToken(appDomain));
    body->setProperty("projectName", projectName);
    body->setProperty("suiteVersion", suiteVersion);
    body->setProperty("appVersion", appVersion);
    const auto json = juce::JSON::toString(juce::var(body), false);
    const juce::MemoryBlock data(json.toRawUTF8(), json.getNumBytesAsUTF8());

    int statusCode = 0;
    auto stream = baseUrl("/project/create").withPOSTData(data).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("POST")
            .withConnectionTimeoutMs(5000)
            .withExtraHeaders("Content-Type: application/json\r\n")
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
    {
        errorMessage = "Could not create the project.";
        return false;
    }

    const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "The service returned an unexpected response.";
        return false;
    }

    outProjectId = object->getProperty("projectId").toString();
    return creation::assets::fromVar(object->getProperty("manifest"), outManifest);
}

bool SuiteVfsServiceClient::readManifest(const juce::String& projectId, creation::assets::ProjectManifest& outManifest) const
{
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = baseUrl("/project/manifest").withParameter("projectId", projectId).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
        return false;

    return creation::assets::fromVar(juce::JSON::parse(stream->readEntireStreamAsString()), outManifest);
}

bool SuiteVfsServiceClient::writeManifest(const juce::String& projectId, const creation::assets::ProjectManifest& manifest) const
{
    if (httpPort_ <= 0)
        return false;

    const auto json = creation::assets::serializeManifest(manifest, false);
    const juce::MemoryBlock data(json.toRawUTF8(), json.getNumBytesAsUTF8());

    int statusCode = 0;
    auto stream = baseUrl("/project/manifest").withParameter("projectId", projectId).withPOSTData(data).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("PUT")
            .withConnectionTimeoutMs(5000)
            .withExtraHeaders("Content-Type: application/json\r\n")
            .withStatusCode(&statusCode));

    return stream != nullptr && statusCode == 200;
}

bool SuiteVfsServiceClient::listProjects(creation::assets::SuiteAppDomain appDomain, juce::Array<ProjectSummary>& outProjects) const
{
    outProjects.clear();
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = baseUrl("/project/list").withParameter("appDomain", creation::assets::toStorageToken(appDomain)).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
        return false;

    const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
    const auto* array = parsed.getArray();
    if (array == nullptr)
        return false;

    for (const auto& entry : *array)
    {
        const auto* object = entry.getDynamicObject();
        if (object == nullptr)
            continue;

        ProjectSummary summary;
        summary.projectId = object->getProperty("projectId").toString();
        creation::assets::fromVar(object->getProperty("manifest"), summary.manifest);
        summary.totalSizeBytes = static_cast<std::int64_t>(static_cast<juce::int64>(object->getProperty("totalSizeBytes")));
        outProjects.add(summary);
    }

    return true;
}

bool SuiteVfsServiceClient::cloneProject(const juce::String& sourceProjectId, const juce::String& newProjectName,
                                         juce::String& outNewProjectId, juce::String& errorMessage) const
{
    if (httpPort_ <= 0)
    {
        errorMessage = "The suite VFS service has not been discovered yet.";
        return false;
    }

    auto* body = new juce::DynamicObject();
    body->setProperty("sourceProjectId", sourceProjectId);
    body->setProperty("newProjectName", newProjectName);
    const auto json = juce::JSON::toString(juce::var(body), false);
    const juce::MemoryBlock data(json.toRawUTF8(), json.getNumBytesAsUTF8());

    int statusCode = 0;
    auto stream = baseUrl("/project/clone").withPOSTData(data).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("POST")
            .withConnectionTimeoutMs(5000)
            .withExtraHeaders("Content-Type: application/json\r\n")
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
    {
        errorMessage = "Could not clone the project.";
        return false;
    }

    const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
    const auto* object = parsed.getDynamicObject();
    if (object == nullptr)
    {
        errorMessage = "The service returned an unexpected response.";
        return false;
    }

    outNewProjectId = object->getProperty("projectId").toString();
    return true;
}

bool SuiteVfsServiceClient::deleteProject(const juce::String& projectId, juce::String& errorMessage) const
{
    if (httpPort_ <= 0)
    {
        errorMessage = "The suite VFS service has not been discovered yet.";
        return false;
    }

    int statusCode = 0;
    auto stream = baseUrl("/project").withParameter("projectId", projectId).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("DELETE")
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
    {
        errorMessage = "Could not delete the project.";
        return false;
    }

    return true;
}

bool SuiteVfsServiceClient::readProjectEntry(const juce::String& projectId, const juce::String& logicalPath, juce::MemoryBlock& outData) const
{
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = projectEntryUrl(projectId, logicalPath).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
        return false;

    outData.reset();
    stream->readIntoMemoryBlock(outData);
    return true;
}

bool SuiteVfsServiceClient::writeProjectEntry(const juce::String& projectId, const juce::String& logicalPath, const juce::MemoryBlock& data) const
{
    if (httpPort_ <= 0)
        return false;

    auto url = projectEntryUrl(projectId, logicalPath).withPOSTData(data);

    int statusCode = 0;
    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("PUT")
            .withConnectionTimeoutMs(5000)
            .withExtraHeaders("Content-Type: application/octet-stream\r\n")
            .withStatusCode(&statusCode));

    return stream != nullptr && statusCode == 200;
}

bool SuiteVfsServiceClient::removeProjectEntry(const juce::String& projectId, const juce::String& logicalPath) const
{
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = projectEntryUrl(projectId, logicalPath).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("DELETE")
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    return stream != nullptr && statusCode == 200;
}

bool SuiteVfsServiceClient::listProjectEntries(const juce::String& projectId, juce::StringArray& outPaths) const
{
    outPaths.clear();
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = baseUrl("/project/entries").withParameter("projectId", projectId).createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withConnectionTimeoutMs(5000)
            .withStatusCode(&statusCode));

    if (stream == nullptr || statusCode != 200)
        return false;

    const auto parsed = juce::JSON::parse(stream->readEntireStreamAsString());
    const auto* array = parsed.getArray();
    if (array == nullptr)
        return false;

    for (const auto& entry : *array)
        outPaths.add(entry.toString());

    return true;
}
}
