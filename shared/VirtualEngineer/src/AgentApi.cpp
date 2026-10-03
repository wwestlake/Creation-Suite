#include <creation/agent/AgentApi.h>

#include <creation/services/SuiteVfsJsonStore.h>

#include <cmath>

#include <mutex>

namespace creation::agent
{
namespace
{
constexpr int maxRequestBytes = 1024 * 1024;

juce::var errorBody(const juce::String& message)
{
    auto* body = new juce::DynamicObject();
    body->setProperty("error", message);
    return juce::var(body);
}

juce::String headerValue(const juce::StringArray& lines, const juce::String& name)
{
    for (int index = 1; index < lines.size(); ++index)
    {
        const auto line = lines[index];
        const auto colon = line.indexOfChar(':');
        if (colon > 0 && line.substring(0, colon).trim().equalsIgnoreCase(name))
            return line.substring(colon + 1).trim();
    }
    return {};
}

juce::String queryValue(const juce::String& query, const juce::String& name)
{
    for (const auto& pair : juce::StringArray::fromTokens(query, "&", ""))
        if (pair.upToFirstOccurrenceOf("=", false, false) == name)
            return juce::URL::removeEscapeChars(pair.fromFirstOccurrenceOf("=", false, false).replaceCharacter('+', ' '));
    return {};
}
} // namespace

struct AgentApi::State
{
    struct Request
    {
        juce::String status { "queued" };
        juce::String response;
        juce::String error;
        juce::var details;
        double startedAtMs = 0.0;
        double durationMs = 0.0;
    };

    std::mutex mutex;
    std::map<juce::String, Request> requests;
    bool active = true;
};

struct AgentApi::HttpRequest
{
    juce::String method;
    juce::String path;
    juce::String query;
    juce::String authorization;
    juce::String body;
};

AgentApi::AgentApi(VirtualEngineer& e) : juce::Thread("Virtual Engineer API"), engineer(e), state(std::make_shared<State>()) {}

AgentApi::~AgentApi()
{
    stop();
}

juce::String AgentApi::discoveryEntry(creation::assets::SuiteAppDomain app)
{
    return "agents/" + creation::assets::toStorageToken(app) + ".json";
}

void AgentApi::addAppEndpoint(const juce::String& name, const juce::String& description, std::function<juce::var()> handler)
{
    endpoints[name] = { description, std::move(handler) };
}

int AgentApi::getPort() const
{
    return listener.getBoundPort();
}

bool AgentApi::start(juce::String& errorMessage)
{
    if (isThreadRunning())
        return true;
    if (! listener.createListener(0, "127.0.0.1"))
    {
        errorMessage = "The Virtual Engineer API could not open a local port.";
        return false;
    }
    token = juce::Uuid().toString();
    auto* discovery = new juce::DynamicObject();
    discovery->setProperty("schema", "djehuti-agent-api");
    discovery->setProperty("version", 1);
    discovery->setProperty("app", creation::assets::toStorageToken(engineer.getApp()));
    discovery->setProperty("baseUrl", "http://127.0.0.1:" + juce::String(listener.getBoundPort()));
    discovery->setProperty("token", token);
    if (! creation::services::SuiteVfsJsonStore::saveJson(discoveryEntry(engineer.getApp()), juce::var(discovery), errorMessage))
    {
        listener.close();
        return false;
    }
    {
        std::lock_guard lock(state->mutex);
        state->active = true;
    }
    startThread();
    return true;
}

void AgentApi::stop()
{
    if (! isThreadRunning())
        return;
    {
        std::lock_guard lock(state->mutex);
        state->active = false;
    }
    signalThreadShouldExit();
    listener.close();
    stopThread(3000);
    // Remove the announcement only if it is still this run's.
    juce::String error;
    const auto entry = discoveryEntry(engineer.getApp());
    if (creation::services::SuiteVfsJsonStore::loadJson(entry, error).getProperty("token", {}).toString() == token)
        creation::services::SuiteVfsJsonStore::removeJson(entry, error);
}

void AgentApi::run()
{
    while (! threadShouldExit())
    {
        std::unique_ptr<juce::StreamingSocket> socket(listener.waitForNextConnection());
        if (socket != nullptr && ! threadShouldExit())
            handle(*socket);
    }
}

bool AgentApi::onMessageThread(std::function<void()> work, int timeoutMs)
{
    if (runDirectlyForTesting)
    {
        work();
        return true;
    }
    auto done = std::make_shared<juce::WaitableEvent>();
    juce::MessageManager::callAsync([work, done] {
        work();
        done->signal();
    });
    return done->wait(timeoutMs);
}

void AgentApi::handle(juce::StreamingSocket& socket)
{
    HttpRequest request;
    if (! readRequest(socket, request))
    {
        writeJson(socket, 400, "Bad Request", errorBody("Invalid HTTP request."));
        return;
    }
    if (request.authorization != "Bearer " + token)
    {
        writeJson(socket, 401, "Unauthorized", errorBody("A valid bearer token is required (see the VFS entry " + discoveryEntry(engineer.getApp()) + ")."));
        return;
    }

    if (request.method == "GET" && request.path == "/v1/status")
    {
        auto* body = new juce::DynamicObject();
        body->setProperty("status", "ready");
        body->setProperty("service", "Djehuti Virtual Engineer API");
        body->setProperty("app", creation::assets::toStorageToken(engineer.getApp()));
        body->setProperty("projectId", engineer.getProjectId());
        body->setProperty("busy", engineer.isBusy());
        juce::Array<juce::var> names;
        for (const auto& [name, endpoint] : endpoints)
            names.add(name);
        body->setProperty("appEndpoints", juce::var(names));
        writeJson(socket, 200, "OK", juce::var(body));
        return;
    }

    if (request.method == "POST" && request.path == "/v1/messages")
    {
        const auto parsed = juce::JSON::parse(request.body);
        const auto content = parsed.getProperty("content", {}).toString().trim();
        if (! parsed.isObject() || content.isEmpty())
        {
            writeJson(socket, 400, "Bad Request", errorBody("The body is JSON with a non-empty \"content\" string."));
            return;
        }
        const auto requestId = juce::Uuid().toString();
        {
            std::lock_guard lock(state->mutex);
            state->requests[requestId] = {};
        }
        auto sharedState = state;
        auto finish = [sharedState, requestId](bool ok, const juce::String& text, const juce::var& details) {
            std::lock_guard lock(sharedState->mutex);
            auto found = sharedState->requests.find(requestId);
            if (found == sharedState->requests.end())
                return;
            found->second.status = ok ? "completed" : "failed";
            found->second.durationMs = juce::Time::getMillisecondCounterHiRes() - found->second.startedAtMs;
            found->second.details = details;
            (ok ? found->second.response : found->second.error) = text;
        };
        auto& eng = engineer;
        const bool queued = onMessageThread([sharedState, requestId, content, finish, &eng] {
            {
                std::lock_guard lock(sharedState->mutex);
                if (! sharedState->active)
                    return;
                auto& entry = sharedState->requests[requestId];
                entry.status = "running";
                entry.startedAtMs = juce::Time::getMillisecondCounterHiRes();
            }
            if (! eng.ask(content, [finish](const AskResult& result) { finish(result.ok, result.ok ? result.text : result.error, result.details); }))
                finish(false, "The Virtual Engineer is busy with another request; try again when it has answered.", {});
        }, 5000);
        if (! queued)
        {
            writeJson(socket, 504, "Gateway Timeout", errorBody("The app did not take the request."));
            return;
        }
        auto* body = new juce::DynamicObject();
        body->setProperty("requestId", requestId);
        body->setProperty("status", "queued");
        writeJson(socket, 202, "Accepted", juce::var(body));
        return;
    }

    const juce::String requestPrefix = "/v1/requests/";
    if (request.method == "GET" && request.path.startsWith(requestPrefix))
    {
        const auto requestId = request.path.substring(requestPrefix.length()).trim();
        State::Request result;
        {
            std::lock_guard lock(state->mutex);
            const auto found = state->requests.find(requestId);
            if (found == state->requests.end())
            {
                writeJson(socket, 404, "Not Found", errorBody("Unknown request id."));
                return;
            }
            result = found->second;
        }
        auto* body = new juce::DynamicObject();
        body->setProperty("requestId", requestId);
        body->setProperty("status", result.status);
        body->setProperty("durationMs", result.durationMs);
        if (result.response.isNotEmpty()) body->setProperty("response", result.response);
        if (result.error.isNotEmpty()) body->setProperty("error", result.error);
        if (! result.details.isVoid()) body->setProperty("details", result.details);
        writeJson(socket, 200, "OK", juce::var(body));
        return;
    }

    if (request.method == "POST" && request.path == "/v1/cancel")
    {
        auto& eng = engineer;
        onMessageThread([&eng] { eng.cancel(); }, 5000);
        auto* body = new juce::DynamicObject();
        body->setProperty("status", "stopped");
        writeJson(socket, 202, "Accepted", juce::var(body));
        return;
    }

    if (request.method == "GET" && request.path == "/v1/cards/match")
    {
        juce::String error;
        const auto retrieval = engineer.retrieveFor(queryValue(request.query, "q"), error);
        auto* body = new juce::DynamicObject();
        juce::Array<juce::var> tokens, cards;
        for (const auto& t : retrieval.tokens)
            tokens.add(t);
        for (const auto& retrieved : retrieval.cards)
        {
            auto* card = new juce::DynamicObject();
            card->setProperty("id", retrieved.card.id);
            card->setProperty("scope", creation::litesemrag::scopeName(retrieved.scope));
            card->setProperty("kind", retrieved.card.kind);
            card->setProperty("title", retrieved.card.title);
            card->setProperty("priority", retrieved.card.priority);
            if (retrieved.meaning >= 0.0f)
                card->setProperty("meaning", std::round(retrieved.meaning * 1000.0f) / 1000.0f);
            juce::Array<juce::var> words;
            for (const auto& word : retrieved.wordsMatched)
                words.add(word);
            card->setProperty("words", juce::var(words));
            cards.add(juce::var(card));
        }
        body->setProperty("matchedBy", retrieval.byMeaning ? "meaning and words" : "words");
        if (retrieval.wordsOnlyBecause.isNotEmpty())
            body->setProperty("wordsOnlyBecause", retrieval.wordsOnlyBecause);
        body->setProperty("tokens", juce::var(tokens));
        body->setProperty("cards", juce::var(cards));
        body->setProperty("context", retrieval.context);
        if (error.isNotEmpty())
            body->setProperty("warning", error);
        writeJson(socket, 200, "OK", juce::var(body));
        return;
    }

    if (request.method == "GET" && request.path == "/v1/app")
    {
        auto* body = new juce::DynamicObject();
        for (const auto& [name, endpoint] : endpoints)
            body->setProperty(name, endpoint.description);
        writeJson(socket, 200, "OK", juce::var(body));
        return;
    }

    const juce::String appPrefix = "/v1/app/";
    if (request.method == "GET" && request.path.startsWith(appPrefix))
    {
        const auto found = endpoints.find(request.path.substring(appPrefix.length()));
        if (found == endpoints.end())
        {
            writeJson(socket, 404, "Not Found", errorBody("The app has no endpoint \"" + request.path.substring(appPrefix.length()) + "\" (see GET /v1/app)."));
            return;
        }
        auto answer = std::make_shared<juce::var>(); // shared: a handler that answers after the timeout writes here harmlessly
        auto handler = found->second.handler;
        if (! onMessageThread([answer, handler] { *answer = handler(); }, 5000))
        {
            writeJson(socket, 504, "Gateway Timeout", errorBody("The app did not answer in time."));
            return;
        }
        writeJson(socket, 200, "OK", *answer);
        return;
    }

    writeJson(socket, 404, "Not Found", errorBody("Unknown endpoint."));
}

bool AgentApi::readRequest(juce::StreamingSocket& socket, HttpRequest& request)
{
    juce::MemoryBlock bytes;
    char buffer[4096] {};
    int headerEnd = -1;
    int expectedSize = -1;
    for (int attempt = 0; attempt < 100 && static_cast<int>(bytes.getSize()) <= maxRequestBytes; ++attempt)
    {
        if (socket.waitUntilReady(true, 100) <= 0)
            continue;
        const auto count = socket.read(buffer, sizeof(buffer), false);
        if (count <= 0)
            break;
        bytes.append(buffer, static_cast<size_t>(count));
        const auto text = juce::String::fromUTF8(static_cast<const char*>(bytes.getData()), static_cast<int>(bytes.getSize()));
        if (headerEnd < 0)
        {
            headerEnd = text.indexOf("\r\n\r\n");
            if (headerEnd >= 0)
                expectedSize = headerEnd + 4 + headerValue(juce::StringArray::fromLines(text.substring(0, headerEnd)), "Content-Length").getIntValue();
        }
        if (headerEnd >= 0 && static_cast<int>(bytes.getSize()) >= expectedSize)
            break;
    }
    if (headerEnd < 0 || expectedSize < 0 || expectedSize > maxRequestBytes || static_cast<int>(bytes.getSize()) < expectedSize)
        return false;

    const auto text = juce::String::fromUTF8(static_cast<const char*>(bytes.getData()), expectedSize);
    const auto headers = juce::StringArray::fromLines(text.substring(0, headerEnd));
    if (headers.isEmpty())
        return false;
    const auto requestLine = juce::StringArray::fromTokens(headers[0], " ", "");
    if (requestLine.size() < 2)
        return false;
    request.method = requestLine[0].toUpperCase();
    request.path = requestLine[1].upToFirstOccurrenceOf("?", false, false);
    request.query = requestLine[1].fromFirstOccurrenceOf("?", false, false);
    request.authorization = headerValue(headers, "Authorization");
    request.body = text.substring(headerEnd + 4);
    return true;
}

bool AgentApi::writeJson(juce::StreamingSocket& socket, int statusCode, const juce::String& statusText, const juce::var& body)
{
    const auto json = juce::JSON::toString(body, false);
    const auto response = "HTTP/1.1 " + juce::String(statusCode) + " " + statusText + "\r\n"
                          "Content-Type: application/json; charset=utf-8\r\n"
                          "Content-Length: " + juce::String(json.getNumBytesAsUTF8()) + "\r\n"
                          "Cache-Control: no-store\r\n"
                          "Connection: close\r\n\r\n" + json;
    const auto utf8 = response.toUTF8();
    const auto total = static_cast<int>(utf8.sizeInBytes() - 1);
    int written = 0;
    while (written < total)
    {
        const auto count = socket.write(utf8.getAddress() + written, total - written);
        if (count <= 0)
            return false;
        written += count;
    }
    return true;
}
} // namespace creation::agent
