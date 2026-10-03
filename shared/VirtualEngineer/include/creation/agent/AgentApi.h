#pragma once

#include <creation/agent/VirtualEngineer.h>
#include <juce_core/juce_core.h>

#include <functional>
#include <map>
#include <memory>

namespace creation::agent
{
// A local HTTP API on an app's Virtual Engineer (shared/VirtualEngineer/README.md), so a developer's tools - or
// another agent - can talk to the engineer and look into the app. Loopback only, a random port, a bearer token per
// run. It announces itself as the VFS suite entry agents/<app>.json ({ baseUrl, token, app }) - never a file on
// the OS - and removes that entry when it stops.
//
//   GET  /v1/status             the app, its open project, whether the engineer is busy, the app's endpoints
//   POST /v1/messages           {"content": "..."} -> 202 {"requestId"}; the request goes to the engineer
//   GET  /v1/requests/{id}      queued | running | completed | failed, with "response" or "error" and "details"
//   POST /v1/cancel             stops waiting for the running request
//   GET  /v1/cards/match?q=...  the cards a request would bring up (tokens, cards, context), nothing sent
//   GET  /v1/app                the app's own endpoints
//   GET  /v1/app/{name}         one of them: whatever the app chose to show (its open graph, its types...)
class AgentApi final : private juce::Thread
{
public:
    AgentApi(VirtualEngineer& engineer);
    ~AgentApi() override;

    // An endpoint for looking into the app, answered on the message thread: GET /v1/app/<name>. Add them before start().
    void addAppEndpoint(const juce::String& name, const juce::String& description, std::function<juce::var()> handler);

    bool start(juce::String& errorMessage);
    void stop();
    bool isRunning() const { return isThreadRunning(); }
    int getPort() const;
    juce::String getToken() const { return token; }

    // The VFS suite entry an app's API announces itself in.
    static juce::String discoveryEntry(creation::assets::SuiteAppDomain app);

    // For tests: run on-message-thread work where it happens (a console test has no message loop).
    bool runDirectlyForTesting = false;

private:
    struct State;
    struct HttpRequest;
    struct Endpoint
    {
        juce::String description;
        std::function<juce::var()> handler;
    };

    void run() override;
    void handle(juce::StreamingSocket& socket);
    bool onMessageThread(std::function<void()> work, int timeoutMs);
    static bool readRequest(juce::StreamingSocket& socket, HttpRequest& request);
    static bool writeJson(juce::StreamingSocket& socket, int statusCode, const juce::String& statusText, const juce::var& body);

    VirtualEngineer& engineer;
    juce::StreamingSocket listener;
    juce::String token;
    std::shared_ptr<State> state;
    std::map<juce::String, Endpoint> endpoints;
};
} // namespace creation::agent
