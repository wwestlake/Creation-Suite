#include <JuceHeader.h>

#include <creation/assets/ProjectSession.h>
#include <creation/assets/SuiteRootProject.h>
#include <creation/services/SuiteProcessRegistry.h>
#include <creation/suite/SuiteSettings.h>

#include <httplib.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

// The suite-owned VFS service (docs/architecture/Suite-Shared-Project-Model.md,
// "Mechanism chosen"): the only process that ever opens the suite root
// project's container directly. Every app is an HTTP/WebSocket client of
// this process instead. Scope for this pass: the root project only --
// regular per-app project containers are unaffected, still opened
// directly by their owning app (see the plan's "explicitly deferred"
// section for why).

namespace
{
constexpr const char* kServiceAppId = "CreationSuiteVfsService";
constexpr double kIdleShutdownGraceSeconds = 20.0;
constexpr int kLivenessCheckIntervalMs = 5000;

void appendBootLog(const std::string& message)
{
    const char* appData = std::getenv("APPDATA");
    const std::string base = appData != nullptr ? appData : "C:\\Users\\wwestlake\\AppData\\Roaming";
    const std::string logsDirectory = base + "\\Creation Suite\\Logs";
    juce::File(logsDirectory).createDirectory();

    std::ofstream out(logsDirectory + "\\CreationSuiteVfsService-boot.log", std::ios::app);
    if (! out.is_open())
        return;

    out << message << std::endl;
}

void appendServiceLog(const juce::String& message)
{
    auto logFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("Creation Suite")
                       .getChildFile("Logs")
                       .getChildFile("CreationSuiteVfsService.log");

    if (! logFile.getParentDirectory().exists())
        logFile.getParentDirectory().createDirectory();

    juce::FileOutputStream stream(logFile);
    if (! stream.openedOk())
        return;

    stream.setPosition(logFile.existsAsFile() ? logFile.getSize() : 0);
    stream.writeText("[" + juce::Time::getCurrentTime().toString(true, true, true, true) + "] " + message + "\n",
                     false,
                     false,
                     "\n");
    stream.flush();
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

bool suiteHasAnyOtherLiveApp()
{
    for (const auto& record : creation::services::SuiteProcessRegistry::EnumerateLiveProcesses())
        if (record.appId != kServiceAppId)
            return true;
    return false;
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
    appendBootLog("main: settings loaded");
    appendServiceLog("startup; suiteVfsRoot=" + settings.suiteVfsRoot
                     + (settingsError.isNotEmpty() ? " settingsError=" + settingsError : ""));

    juce::CriticalSection sessionLock;
    creation::assets::ProjectSession rootSession;
    {
        juce::String rootError;
        const juce::ScopedLock lock(sessionLock);
        appendBootLog("main: opening suite root");
        if (! creation::assets::SuiteRootProject::openOrCreate(settings, rootSession, rootError))
        {
            appendBootLog("main: suite root open/create failed");
            appendServiceLog("failed to open/create suite root project: " + rootError);
            std::cerr << "[vfs-service] failed to open/create the suite root project: " << rootError << std::endl;
            return 1;
        }
    }
    appendBootLog("main: suite root ready");
    appendServiceLog("suite root project ready");

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
            const juce::ScopedLock lock(sessionLock);
            if (! rootSession.readEntry(path, data))
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

        juce::String commitError;
        {
            const juce::ScopedLock lock(sessionLock);
            if (! rootSession.writeEntry(path, data))
            {
                res.status = 500;
                res.set_content("Could not write the entry.", "text/plain");
                return;
            }
            if (! rootSession.commit(commitError))
            {
                res.status = 500;
                res.set_content(commitError.toStdString(), "text/plain");
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

        juce::String commitError;
        {
            const juce::ScopedLock lock(sessionLock);
            rootSession.removeEntry(path);
            if (! rootSession.commit(commitError))
            {
                res.status = 500;
                res.set_content(commitError.toStdString(), "text/plain");
                return;
            }
        }

        broadcastEntryChanged(wsClients, wsClientsLock, path);
        res.set_content("{\"status\":\"ok\"}", "application/json");
    });

    http.Get("/suite/entries", [&](const httplib::Request&, httplib::Response& res)
    {
        const juce::ScopedLock lock(sessionLock);
        const auto paths = rootSession.listEntryPaths();

        juce::Array<juce::var> array;
        for (const auto& path : paths)
            array.add(path);

        res.set_content(juce::JSON::toString(juce::var(array), false).toStdString(), "application/json");
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
    registration.RegisterSelf(kServiceAppId, /*oscPort*/ 0, /*pipeName*/ {}, httpPort);
    appendBootLog("main: process registered");

    appendServiceLog("listening; http=" + juce::String(httpPort) + " ws=" + juce::String(wsPort));
    std::cout << "[vfs-service] listening: http=" << httpPort << " ws=" << wsPort << std::endl;

    // Lifecycle: no app launched this process to own its lifetime the way
    // an app owns its own window -- it exits itself once no other suite
    // app is left running, after a grace period so quick app-switches
    // don't thrash a restart.
    double secondsSinceLastSeenAnotherApp = 0.0;
    for (;;)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(kLivenessCheckIntervalMs));

        if (suiteHasAnyOtherLiveApp())
        {
            secondsSinceLastSeenAnotherApp = 0.0;
            continue;
        }

        secondsSinceLastSeenAnotherApp += kLivenessCheckIntervalMs / 1000.0;
        if (secondsSinceLastSeenAnotherApp >= kIdleShutdownGraceSeconds)
        {
            appendServiceLog("idle shutdown");
            std::cout << "[vfs-service] no other suite app running; shutting down." << std::endl;
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
    return 0;
}
