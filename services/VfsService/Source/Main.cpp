#include <JuceHeader.h>

#include "VfsProjectStore.h"
#include "VfsChunkUploadRoutes.h"

#include <creation/assets/ProjectManifest.h>
#include <creation/services/SuiteProcessRegistry.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/suite/SuiteStoragePaths.h>

#include <httplib.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <atomic>
#include <deque>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

// The suite-owned VFS service (docs/architecture/Suite-VFS-Single-Container-Plan.md):
// the only process that ever opens the VFS. The whole VFS is ONE container file,
// vfs.bin, in the configured VFS root (an exFAT volume, see shared/VFS), reached
// through VfsProjectStore; nothing else is stored on the OS disk. Every app is an HTTP/WebSocket
// client of this process instead, for both suite-level entries and per-app project
// storage alike -- there is no separate "per-app containers are still opened
// directly by their owning app" carve-out anymore.

namespace
{
constexpr const char* kServiceAppId = "CreationSuiteVfsService";
// The service closes itself after an hour with no requests and no connected app. An app that finds it gone starts it
// again (SuiteVfsServiceClient::discover).
constexpr double kIdleShutdownSeconds = 60.0 * 60.0;
constexpr int kLivenessCheckIntervalMs = 5000;

// The service's log is kept in memory (the last few hundred lines) and saved as an entry inside the container
// ("suite/logs/vfs-service.log"). It never becomes a file or folder on the OS disk.
juce::CriticalSection logLock;
std::deque<juce::String> logLines;
bool logChangedSinceFlush = false;
constexpr size_t kMaxLogLines = 500;

void appendServiceLog(const juce::String& message)
{
    const juce::ScopedLock lock(logLock);
    logLines.push_back("[" + juce::Time::getCurrentTime().toString(true, true, true, true) + "] " + message);
    while (logLines.size() > kMaxLogLines)
        logLines.pop_front();
    logChangedSinceFlush = true;
}

void appendBootLog(const std::string& message)
{
    appendServiceLog(juce::String(message));
}

void flushLogToStore(VfsProjectStore& store, juce::CriticalSection& storeLock)
{
    juce::String text;
    {
        const juce::ScopedLock lock(logLock);
        if (! logChangedSinceFlush)
            return;

        for (const auto& line : logLines)
            text << line << "\n";
        logChangedSinceFlush = false;
    }

    const juce::MemoryBlock data(text.toRawUTF8(), text.getNumBytesAsUTF8());
    const juce::ScopedLock lock(storeLock);
    store.writeSuiteEntry("suite/logs/vfs-service.log", data);
}

// Every entry this service manages lives under "suite/" inside the root
// project's container -- keeps this API scoped to suite-level data, not a
// general "any path in the container" passthrough.
juce::String normalizeEntryPath(const juce::String& rawPath)
{
    auto trimmed = rawPath.trim();
    while (trimmed.startsWithChar('/'))
        trimmed = trimmed.substring(1);
    if (! trimmed.startsWith("suite/"))
        trimmed = "suite/" + trimmed;
    return trimmed;
}

void broadcastEntryChanged(std::vector<std::weak_ptr<ix::WebSocket>>& clients,
                           juce::CriticalSection& clientsLock,
                           const juce::String& path)
{
    juce::DynamicObject::Ptr message(new juce::DynamicObject());
    message->setProperty("event", "entryChanged");
    message->setProperty("path", path);
    const auto json = juce::JSON::toString(juce::var(message.get()), false).toStdString();

    const juce::ScopedLock lock(clientsLock);
    for (auto it = clients.begin(); it != clients.end();)
    {
        if (auto client = it->lock())
        {
            client->send(json);
            ++it;
        }
        else
        {
            it = clients.erase(it);
        }
    }
}

}

int main(int, char*[])
{
    appendBootLog("main: entered");
    // Deliberately the first local declared, so it's the LAST thing torn
    // down on the way out of main() (locals unwind in reverse declaration
    // order). Everything below that touches a JUCE facility (juce::File,
    // juce::Thread, juce::CriticalSection, etc.) in its own destructor --
    // the session, the process registration -- must be declared AFTER
    // this, so it destructs BEFORE JUCE itself is torn down. Getting this
    // order wrong is exactly what caused an earlier version of this file
    // to crash after main() had already returned: a project session held
    // in a file-scope global outlived this initialiser and tried to use
    // JUCE facilities that no longer existed.
    juce::ScopedJuceInitialiser_GUI juceInit; // pulls in message loop/timer plumbing shared code relies on -- same pattern CreationEngineServer uses for its own headless console app.
    appendBootLog("main: juce init complete");

    juce::String settingsError;
    const auto settings = creation::suite::SuiteSettingsStore().load(settingsError);
    if (! creation::suite::hasStorageRoot(settings))
        return 3; // no VFS root chosen: the service has nowhere to keep anything and must not invent one

    appendBootLog("main: settings loaded");
    appendServiceLog("startup; suiteVfsRoot=" + settings.suiteVfsRoot
                     + (settingsError.isNotEmpty() ? " settingsError=" + settingsError : ""));

    juce::CriticalSection storeLock;
    VfsProjectStore store(settings);
    if (! store.isReady())
    {
        appendServiceLog("cannot start: " + store.startupError());
        return 4;
    }
    appendBootLog("main: project store ready");
    appendServiceLog("project store ready; container=" + creation::suite::getVfsContainerFile(settings).getFullPathName());
    flushLogToStore(store, storeLock);

    ix::initNetSystem();
    appendBootLog("main: net init complete");

    std::vector<std::weak_ptr<ix::WebSocket>> wsClients;
    juce::CriticalSection wsClientsLock;

    httplib::Server http;

    http.Get("/health", [](const httplib::Request&, httplib::Response& res)
    {
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Get("/suite/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        const auto path = normalizeEntryPath(juce::String(req.get_param_value("path")));
        juce::MemoryBlock data;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.readSuiteEntry(path, data))
            {
                res.status = 404;
                return;
            }
        }
        res.set_content(static_cast<const char*>(data.getData()), data.getSize(), "application/octet-stream");
    });

    http.Put("/suite/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        const auto path = normalizeEntryPath(juce::String(req.get_param_value("path")));
        const juce::MemoryBlock data(req.body.data(), req.body.size());

        {
            const juce::ScopedLock lock(storeLock);
            if (! store.writeSuiteEntry(path, data))
            {
                res.status = 500;
                res.set_content("Could not write the entry.", "text/plain");
                return;
            }
        }

        broadcastEntryChanged(wsClients, wsClientsLock, path);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Delete("/suite/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        const auto path = normalizeEntryPath(juce::String(req.get_param_value("path")));

        {
            const juce::ScopedLock lock(storeLock);
            if (! store.removeSuiteEntry(path))
            {
                res.status = 404;
                return;
            }
        }

        broadcastEntryChanged(wsClients, wsClientsLock, path);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Get("/suite/entries", [&](const httplib::Request&, httplib::Response& res)
    {
        const juce::ScopedLock lock(storeLock);
        const auto paths = store.listSuiteEntryPaths();

        juce::Array<juce::var> array;
        for (const auto& path : paths)
            array.add(path);

        res.set_content(juce::JSON::toString(juce::var(array), false).toStdString(), "application/json");
    });

    // --- Project storage: folders inside the container, keyed by projectId, never a real
    // path handed back to a caller (see docs/architecture/Suite-Shared-Project-Model.md).

    http.Post("/project/create", [&](const httplib::Request& req, httplib::Response& res)
    {
        const auto body = juce::JSON::parse(juce::String(req.body));
        const auto* object = body.getDynamicObject();
        if (object == nullptr)
        {
            res.status = 400;
            return;
        }

        const auto appDomain = creation::assets::suiteAppDomainFromStorageToken(object->getProperty("appDomain").toString());
        const auto projectName = object->getProperty("projectName").toString();
        const auto suiteVersion = object->getProperty("suiteVersion").toString();
        const auto appVersion = object->getProperty("appVersion").toString();

        juce::String projectId, errorMessage;
        creation::assets::ProjectManifest manifest;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.createProject(appDomain, projectName, suiteVersion, appVersion, projectId, manifest, errorMessage))
            {
                res.status = 500;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        auto* resultObject = new juce::DynamicObject();
        resultObject->setProperty("projectId", projectId);
        resultObject->setProperty("manifest", creation::assets::toVar(manifest));
        res.set_content(juce::JSON::toString(juce::var(resultObject), false).toStdString(), "application/json");
    });

    http.Get("/project/manifest", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId"))
        {
            res.status = 400;
            return;
        }

        creation::assets::ProjectManifest manifest;
        juce::String errorMessage;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.readManifest(juce::String(req.get_param_value("projectId")), manifest, errorMessage))
            {
                res.status = 404;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        res.set_content(juce::JSON::toString(creation::assets::toVar(manifest), false).toStdString(), "application/json");
    });

    http.Put("/project/manifest", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId"))
        {
            res.status = 400;
            return;
        }

        creation::assets::ProjectManifest manifest;
        juce::String parseError;
        if (! creation::assets::deserializeManifest(juce::String(req.body), manifest, parseError))
        {
            res.status = 400;
            res.set_content(parseError.toStdString(), "text/plain");
            return;
        }

        juce::String errorMessage;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.writeManifest(juce::String(req.get_param_value("projectId")), manifest, errorMessage))
            {
                res.status = 500;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Get("/project/list", [&](const httplib::Request&, httplib::Response& res)
    {
        // Unfiltered by design -- projects are not owned by any app. See
        // docs/architecture/Suite-Shared-Project-Model.md.
        juce::Array<VfsProjectStore::ProjectSummary> summaries;
        {
            const juce::ScopedLock lock(storeLock);
            store.listProjects(summaries);
        }

        juce::Array<juce::var> array;
        for (const auto& summary : summaries)
        {
            auto* entry = new juce::DynamicObject();
            entry->setProperty("projectId", summary.projectId);
            entry->setProperty("manifest", creation::assets::toVar(summary.manifest));
            entry->setProperty("totalSizeBytes", static_cast<juce::int64>(summary.totalSizeBytes));
            array.add(juce::var(entry));
        }

        res.set_content(juce::JSON::toString(juce::var(array), false).toStdString(), "application/json");
    });

    http.Delete("/project", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId"))
        {
            res.status = 400;
            return;
        }

        juce::String errorMessage;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.deleteProject(juce::String(req.get_param_value("projectId")), errorMessage))
            {
                res.status = 404;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Post("/project/clone", [&](const httplib::Request& req, httplib::Response& res)
    {
        const auto body = juce::JSON::parse(juce::String(req.body));
        const auto* object = body.getDynamicObject();
        if (object == nullptr)
        {
            res.status = 400;
            return;
        }

        juce::String newProjectId, errorMessage;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.cloneProject(object->getProperty("sourceProjectId").toString(),
                                     object->getProperty("newProjectName").toString(),
                                     newProjectId, errorMessage))
            {
                res.status = 500;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        auto* resultObject = new juce::DynamicObject();
        resultObject->setProperty("projectId", newProjectId);
        res.set_content(juce::JSON::toString(juce::var(resultObject), false).toStdString(), "application/json");
    });

    http.Get("/project/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId") || ! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        juce::MemoryBlock data;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.readEntry(juce::String(req.get_param_value("projectId")), juce::String(req.get_param_value("path")), data))
            {
                res.status = 404;
                return;
            }
        }
        res.set_content(static_cast<const char*>(data.getData()), data.getSize(), "application/octet-stream");
    });

    registerVfsChunkUploadRoutes(http, store, storeLock, [&](const juce::String& projectAndPath)
    {
        broadcastEntryChanged(wsClients, wsClientsLock, projectAndPath);
    });

    http.Put("/project/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId") || ! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        const auto projectId = juce::String(req.get_param_value("projectId"));
        const auto path = juce::String(req.get_param_value("path"));
        const juce::MemoryBlock data(req.body.data(), req.body.size());

        juce::String errorMessage;
        {
            const juce::ScopedLock lock(storeLock);
            if (! store.writeEntry(projectId, path, data, errorMessage))
            {
                res.status = 500;
                res.set_content(errorMessage.toStdString(), "text/plain");
                return;
            }
        }

        broadcastEntryChanged(wsClients, wsClientsLock, projectId + ":" + path);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Delete("/project/entry", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId") || ! req.has_param("path"))
        {
            res.status = 400;
            return;
        }

        const auto projectId = juce::String(req.get_param_value("projectId"));
        const auto path = juce::String(req.get_param_value("path"));

        {
            const juce::ScopedLock lock(storeLock);
            if (! store.removeEntry(projectId, path))
            {
                res.status = 404;
                return;
            }
        }

        broadcastEntryChanged(wsClients, wsClientsLock, projectId + ":" + path);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Get("/project/entries", [&](const httplib::Request& req, httplib::Response& res)
    {
        if (! req.has_param("projectId"))
        {
            res.status = 400;
            return;
        }

        juce::StringArray paths;
        {
            const juce::ScopedLock lock(storeLock);
            paths = store.listEntryPaths(juce::String(req.get_param_value("projectId")));
        }

        juce::Array<juce::var> array;
        for (const auto& path : paths)
            array.add(path);

        res.set_content(juce::JSON::toString(juce::var(array), false).toStdString(), "application/json");
    });

    // Any request counts as activity.
    std::atomic<juce::int64> lastActivityMs { (juce::int64) juce::Time::getMillisecondCounter() };
    http.set_pre_routing_handler([&lastActivityMs](const httplib::Request&, httplib::Response&)
    {
        lastActivityMs.store((juce::int64) juce::Time::getMillisecondCounter());
        return httplib::Server::HandlerResponse::Unhandled;
    });

    const int httpPort = http.bind_to_any_port("127.0.0.1");
    appendBootLog("main: http bound");
    std::thread httpThread([&http] { http.listen_after_bind(); });

    const int wsPort = httpPort + 1;
    ix::WebSocketServer wsServer(wsPort, "127.0.0.1");
    wsServer.setOnConnectionCallback(
        [&wsClients, &wsClientsLock](std::weak_ptr<ix::WebSocket> webSocket, std::shared_ptr<ix::ConnectionState>)
        {
            const juce::ScopedLock lock(wsClientsLock);
            wsClients.push_back(webSocket);
        });

    const auto wsListenResult = wsServer.listen();
    if (! wsListenResult.first)
    {
        appendBootLog("main: websocket listen failed");
        appendServiceLog("failed to start websocket server: " + juce::String(wsListenResult.second));
        std::cerr << "[vfs-service] failed to start the WebSocket server: " << wsListenResult.second << std::endl;
        return 1;
    }
    wsServer.start();
    appendBootLog("main: websocket started");

    creation::services::SuiteProcessRegistration registration;
    registration.SetServiceInfo(creation::services::vfsServiceProtocol, JUCE_APPLICATION_VERSION_STRING);
    registration.RegisterSelf(kServiceAppId, /*oscPort*/ 0, /*pipeName*/ {}, httpPort);
    appendBootLog("main: process registered");

    appendServiceLog("listening; http=" + juce::String(httpPort) + " ws=" + juce::String(wsPort));
    std::cout << "[vfs-service] listening: http=" << httpPort << " ws=" << wsPort << std::endl;

    // Lifecycle: nothing launched this process to own its lifetime, so it closes itself after an hour of inactivity
    // (no requests, and no app connected over the WebSocket).
    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kLivenessCheckIntervalMs));
        flushLogToStore(store, storeLock);

        bool anyAppConnected = false;
        {
            const juce::ScopedLock lock(wsClientsLock);
            for (const auto& client : wsClients)
                if (client.lock() != nullptr)
                    anyAppConnected = true;
        }

        if (anyAppConnected)
        {
            lastActivityMs.store((juce::int64) juce::Time::getMillisecondCounter());
            continue;
        }

        const auto idleSeconds = (double) ((juce::int64) juce::Time::getMillisecondCounter() - lastActivityMs.load()) / 1000.0;
        if (idleSeconds >= kIdleShutdownSeconds)
        {
            appendServiceLog("idle shutdown after an hour without requests");
            std::cout << "[vfs-service] idle for an hour; shutting down." << std::endl;
            break;
        }
    }

    // Every write/delete handler above already commits synchronously, so
    // there is nothing left to flush here -- just tear the servers down
    // in the reverse order they were brought up.
    wsServer.stop();
    http.stop();
    httpThread.join();
    ix::uninitNetSystem();

    appendServiceLog("shutdown complete");
    flushLogToStore(store, storeLock);
    return 0;
}
