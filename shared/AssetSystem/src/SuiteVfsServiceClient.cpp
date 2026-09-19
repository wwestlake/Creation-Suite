#include "creation/services/SuiteVfsServiceClient.h"

#include <creation/services/SuiteProcessRegistry.h>
#include <creation/suite/SuiteSettings.h>

#include <atomic>
#include <mutex>

namespace
{
constexpr const char* kServiceAppId = "CreationSuiteVfsService";

// Real bug fixed here: discover() used to be a genuine no-op only within a
// single client instance's own lifetime. Every SuiteVfsJsonStore/
// ProjectContainerService call constructs its own throwaway
// SuiteVfsServiceClient, so when the service isn't already registered, each
// of the many independent calls made during a single app startup (settings
// load, project list, control-surface mappings, settings save, ...)
// redundantly spawned another copy of the service process and blocked the
// calling thread -- the message thread, during MainComponent's constructor
// -- for up to its own fresh 10-second poll. Stacked across ~6+ call sites
// that's a full minute or more of "Not Responding", which is what actually
// produced the startup hang, not any one blocking call by itself. Caching
// the launch attempt process-wide makes that spawn+poll happen at most once
// per run; the cheap registry scan below still runs every call so a service
// that comes up later (or was already found) is still picked up instantly.
std::mutex& discoveryMutex()
{
    static std::mutex m;
    return m;
}

std::atomic<bool>& launchAttempted()
{
    static std::atomic<bool> b { false };
    return b;
}

// Real bug fixed here: this used to be a hardcoded dev-tree literal
// ("D:/CreationSuite-Workspaces/codex-{debug,release}-bin"), unreachable
// by anything outside this exact dev machine's build layout -- including
// an external process (a Blender add-on) that needs the same launch
// capability. Now reads suiteExecutablesRoot from the suite's own
// settings file (SuiteSettings.h), whose default value matches the old
// hardcoded literal exactly, so behavior is unchanged until someone
// actually configures a different location.
juce::File findServiceExecutable()
{
    juce::String loadError;
    const auto settings = creation::suite::SuiteSettingsStore().load(loadError);
    return juce::File(settings.suiteExecutablesRoot).getChildFile("DjehutiSuiteVfsService.exe");
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

    // Someone else already tried (and failed) to launch it this run -- don't
    // pile on another spawn-and-wait. The scan above already covers the case
    // where it came up in the meantime.
    if (launchAttempted().load(std::memory_order_acquire))
        return false;

    std::lock_guard<std::mutex> lock(discoveryMutex());
    if (launchAttempted().load(std::memory_order_acquire))
        return false;

    for (const auto& record : creation::services::SuiteProcessRegistry::EnumerateLiveProcesses())
    {
        if (record.appId == kServiceAppId && record.httpPort > 0)
        {
            httpPort_ = record.httpPort;
            return true;
        }
    }

    launchAttempted().store(true, std::memory_order_release);

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

bool SuiteVfsServiceClient::listProjects(juce::Array<ProjectSummary>& outProjects) const
{
    outProjects.clear();
    if (httpPort_ <= 0)
        return false;

    int statusCode = 0;
    auto stream = baseUrl("/project/list").createInputStream(
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

namespace
{
constexpr juce::int64 kChunkBytes = 16 * 1024 * 1024;
// Anything bigger than this goes up in pieces; the service refuses a single request over 100 MB, and a
// piece is also what lets a big upload report progress and be cancelled.
constexpr juce::int64 kSingleRequestLimit = 24 * 1024 * 1024;
}

bool SuiteVfsServiceClient::readProjectEntryToFile(const juce::String& projectId, const juce::String& logicalPath,
                                                   const juce::File& destination, const ProgressFn& progress) const
{
    lastReadError_ = {};

    if (httpPort_ <= 0)
    {
        lastReadError_ = "the project service is not running";
        return false;
    }

    if (! destination.getParentDirectory().createDirectory())
    {
        lastReadError_ = "the destination folder could not be created";
        return false;
    }

    destination.deleteFile();
    auto out = std::make_unique<juce::FileOutputStream>(destination);
    if (out->failedToOpen())
    {
        lastReadError_ = "the destination file could not be opened for writing";
        return false;
    }

    juce::int64 offset = 0;
    juce::int64 total = -1;

    do
    {
        bool got = false;
        int statusCode = 0;
        for (int attempt = 0; attempt < 3 && ! got; ++attempt)
        {
            juce::StringPairArray headers;
            statusCode = 0;
            auto url = baseUrl("/project/entry/range")
                           .withParameter("projectId", projectId)
                           .withParameter("path", logicalPath)
                           .withParameter("offset", juce::String(offset))
                           .withParameter("length", juce::String(kChunkBytes));

            auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                    .withConnectionTimeoutMs(120000)
                                                    .withResponseHeaders(&headers)
                                                    .withStatusCode(&statusCode));
            if (stream == nullptr)
                continue;

            if (statusCode != 200)
                break;

            juce::MemoryBlock piece;
            stream->readIntoMemoryBlock(piece);
            total = headers.getValue("X-Total-Size", "-1").getLargeIntValue();
            if (total < 0)
                break;

            if (piece.getSize() > 0 && ! out->write(piece.getData(), piece.getSize()))
            {
                lastReadError_ = "the destination file could not be written (disk full?)";
                out.reset();
                destination.deleteFile();
                return false;
            }
            offset += (juce::int64) piece.getSize();
            got = true;
            if (piece.getSize() == 0)
                total = offset; // nothing more to read
        }

        // A service that predates ranged reads answers 404 to the range request: fall back to the plain read it does have.
        if (! got && statusCode == 404 && offset == 0)
        {
            juce::MemoryBlock whole;
            if (readProjectEntry(projectId, logicalPath, whole))
            {
                if (whole.getSize() > 0 && ! out->write(whole.getData(), whole.getSize()))
                {
                    lastReadError_ = "the destination file could not be written (disk full?)";
                    out.reset();
                    destination.deleteFile();
                    return false;
                }
                out->flush();
                if (progress)
                    progress(1.0);
                return true;
            }
        }

        if (! got)
        {
            lastReadError_ = statusCode == 404
                               ? "the project service could not find that file, or is out of date and cannot send large files - restart Djehuti Station and its project service"
                               : "the project service stopped answering during the download";
            out.reset();
            destination.deleteFile();
            return false;
        }

        if (progress && ! progress(total > 0 ? (double) offset / (double) total : 1.0))
        {
            lastReadError_ = "cancelled";
            out.reset();
            destination.deleteFile();
            return false;
        }
    } while (offset < total);

    out->flush();
    return true;
}

bool SuiteVfsServiceClient::writeProjectEntryFromFile(const juce::String& projectId, const juce::String& logicalPath,
                                                      const juce::File& sourceFile, const ProgressFn& progress) const
{
    lastWriteError_ = {};
    lastWriteCancelled_ = false;

    if (httpPort_ <= 0)
    {
        lastWriteError_ = "the project service is not running";
        return false;
    }

    juce::FileInputStream input(sourceFile);
    if (input.failedToOpen())
    {
        lastWriteError_ = "the file could not be opened for reading";
        return false;
    }

    const auto total = input.getTotalLength();

    // Small files go up as one plain request - the same call an older project service already understands, so a
    // newer app keeps working against a service that predates chunked uploads.
    if (total > 0 && total <= kSingleRequestLimit)
    {
        juce::MemoryBlock whole;
        if (input.readIntoMemoryBlock(whole) != total)
        {
            lastWriteError_ = "the file could not be read (it may have been moved or changed)";
            return false;
        }

        if (! writeProjectEntry(projectId, logicalPath, whole))
            return false;

        if (progress)
            progress(1.0);
        return true;
    }

    juce::MemoryBlock piece;
    juce::int64 offset = 0;

    const auto discardPartial = [&]
    {
        auto url = baseUrl("/project/entry/chunk").withParameter("projectId", projectId).withParameter("path", logicalPath);
        int status = 0;
        auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                .withHttpRequestCmd("DELETE")
                                                .withConnectionTimeoutMs(5000)
                                                .withStatusCode(&status));
        juce::ignoreUnused(stream);
    };

    do
    {
        const auto thisPiece = juce::jmin(kChunkBytes, total - offset);
        piece.setSize((size_t) thisPiece);
        if (thisPiece > 0 && input.read(piece.getData(), (int) thisPiece) != (int) thisPiece)
        {
            lastWriteError_ = "the file could not be read (it may have been moved or changed)";
            discardPartial();
            return false;
        }

        bool sent = false;
        int statusCode = 0;
        juce::String reply;
        for (int attempt = 0; attempt < 3 && ! sent; ++attempt)
        {
            auto url = baseUrl("/project/entry/chunk")
                           .withParameter("projectId", projectId)
                           .withParameter("path", logicalPath)
                           .withParameter("offset", juce::String(offset))
                           .withParameter("total", juce::String(total))
                           .withPOSTData(piece);

            statusCode = 0;
            auto stream = url.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withHttpRequestCmd("PUT")
                    .withConnectionTimeoutMs(120000)
                    .withExtraHeaders("Content-Type: application/octet-stream\r\n")
                    .withStatusCode(&statusCode));

            if (stream == nullptr)
                continue; // the connection dropped; the same piece is safe to send again

            reply = stream->readEntireStreamAsString().substring(0, 160).trim();
            if (statusCode == 200)
                sent = true;
            else
                break; // a definite answer (out of order, disk full, ...) will not improve by retrying
        }

        if (! sent)
        {
            if (statusCode == 404)
                lastWriteError_ = "the project service is out of date and cannot take large files - close Djehuti Station and its project service, then start it again";
            else if (statusCode == 0)
                lastWriteError_ = "the project service stopped answering during the upload";
            else
                lastWriteError_ = "the project service refused the upload (HTTP " + juce::String(statusCode)
                                + (reply.isNotEmpty() ? ": " + reply : juce::String()) + ")";
            discardPartial();
            return false;
        }

        offset += thisPiece;

        if (progress && ! progress(total > 0 ? (double) offset / (double) total : 1.0))
        {
            lastWriteCancelled_ = true;
            lastWriteError_ = "cancelled";
            if (offset < total)
                discardPartial();
            return false;
        }
    } while (offset < total);

    return true;
}

bool SuiteVfsServiceClient::writeProjectEntry(const juce::String& projectId, const juce::String& logicalPath, const juce::MemoryBlock& data) const
{
    lastWriteError_ = {};
    lastWriteCancelled_ = false;

    if ((juce::int64) data.getSize() > kSingleRequestLimit && httpPort_ > 0)
    {
        // Too big for one request: spool it to a temporary file and stream it up in pieces.
        juce::TemporaryFile temp;
        if (! temp.getFile().replaceWithData(data.getData(), data.getSize()))
        {
            lastWriteError_ = "a temporary copy of the data could not be written";
            return false;
        }
        return writeProjectEntryFromFile(projectId, logicalPath, temp.getFile());
    }

    if (httpPort_ <= 0)
    {
        lastWriteError_ = "the project service is not running";
        return false;
    }

    auto url = projectEntryUrl(projectId, logicalPath).withPOSTData(data);

    // The whole entry goes up in one request, so a big file (a video) needs far longer than the usual 5 s:
    // allow about a second per 10 MB on top of the base, capped at ten minutes.
    const auto timeoutMs = 5000 + (int) juce::jmin<juce::int64>(600000, (juce::int64) (data.getSize() / 10000));

    int statusCode = 0;
    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withHttpRequestCmd("PUT")
            .withConnectionTimeoutMs(timeoutMs)
            .withExtraHeaders("Content-Type: application/octet-stream\r\n")
            .withStatusCode(&statusCode));

    if (stream == nullptr)
    {
        lastWriteError_ = "the project service did not answer the upload of "
                        + juce::File::descriptionOfSizeInBytes((juce::int64) data.getSize())
                        + " (the file may be too large, or the service stopped)";
        return false;
    }

    if (statusCode != 200)
    {
        const auto reply = stream->readEntireStreamAsString().substring(0, 160).trim();
        lastWriteError_ = "the project service answered HTTP " + juce::String(statusCode)
                        + (reply.isNotEmpty() ? " (" + reply + ")" : juce::String());
        return false;
    }

    return true;
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
