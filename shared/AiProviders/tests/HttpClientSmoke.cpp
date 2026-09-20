// The real HTTP transport against a small server on this machine: a reply, a rate limit with Retry-After,
// nothing listening, and a cancel while the server is slow to answer. No internet, no keys.

#include <creation/ai/HttpClient.h>

#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace
{
int failures = 0;

void check(bool ok, const std::string& what)
{
    std::cout << (ok ? "PASS  " : "FAIL  ") << what << std::endl;
    if (! ok)
        ++failures;
}

// Serves one connection at a time: reads a whole request, answers by path.
class MockServer
{
public:
    MockServer()
    {
        listener.createListener(0, "127.0.0.1");
        port = listener.getBoundPort();
        thread = std::thread([this] { serve(); });
    }

    ~MockServer()
    {
        stopping = true;
        listener.close();
        if (thread.joinable())
            thread.join();
    }

    int getPort() const { return port; }
    std::string lastBody;
    std::string lastRequestHead;

private:
    void serve()
    {
        while (! stopping)
        {
            std::unique_ptr<juce::StreamingSocket> client(listener.waitForNextConnection());
            if (client == nullptr)
                continue;

            std::string request;
            char c = 0;
            while (request.find("\r\n\r\n") == std::string::npos && client->read(&c, 1, true) == 1)
                request += c;

            // Header names are case-insensitive: find Content-Length however it was written.
            size_t contentLength = 0;
            std::string lowered = request;
            for (auto& ch : lowered)
                ch = (char) std::tolower((unsigned char) ch);
            const auto lengthPos = lowered.find("content-length: ");
            if (lengthPos != std::string::npos)
                contentLength = (size_t) std::stoul(request.substr(lengthPos + 16));

            std::string body;
            while (body.size() < contentLength && client->read(&c, 1, true) == 1)
                body += c;
            lastBody = body;
            lastRequestHead = request;

            std::string reply;
            if (request.rfind("POST /ok", 0) == 0)
                reply = reply200("{\"answer\":\"hello\"}");
            else if (request.rfind("POST /limited", 0) == 0)
                reply = "HTTP/1.1 429 Too Many Requests\r\nRetry-After: 7\r\nContent-Type: application/json\r\nContent-Length: 30\r\nConnection: close\r\n\r\n{\"error\":{\"message\":\"slow down\"}}";
            else if (request.rfind("POST /slow", 0) == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(2500));
                reply = reply200("{\"late\":true}");
            }
            else
                reply = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

            client->write(reply.data(), (int) reply.size());
            client->close();
        }
    }

    static std::string reply200(const std::string& body)
    {
        return "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size())
               + "\r\nConnection: close\r\n\r\n" + body;
    }

    juce::StreamingSocket listener;
    int port = 0;
    std::atomic<bool> stopping { false };
    std::thread thread;
};
}

int main()
{
    using namespace creation::ai;

    MockServer server;
    JuceHttpClient client;
    CancelToken cancel;
    const std::string base = "http://127.0.0.1:" + std::to_string(server.getPort());

    {
        HttpRequest request;
        request.url = base + "/ok";
        request.headers.push_back({ "Content-Type", "application/json" });
        request.headers.push_back({ "Authorization", "Bearer test-key" });
        request.body = "{\"hello\":\"world\"}";
        auto response = client.post(request, cancel);
        check(response.status == 200 && response.body == "{\"answer\":\"hello\"}", "a reply comes back with its status and body");
        check(server.lastBody == "{\"hello\":\"world\"}", "the body was POSTed as given");
        check(server.lastRequestHead.find("Authorization: Bearer test-key") != std::string::npos, "the headers were sent");
    }

    {
        HttpRequest request;
        request.url = base + "/limited";
        request.body = "{}";
        auto response = client.post(request, cancel);
        check(response.status == 429 && response.retryAfter == "7", "an error status and its Retry-After header are returned");
    }

    {
        HttpRequest request;
        request.url = "http://127.0.0.1:1/nothing";   // nothing listens here
        request.body = "{}";
        request.timeoutSeconds = 5.0;
        auto response = client.post(request, cancel);
        check(response.status == 0 && ! response.networkError.empty() && ! response.cancelled, "nothing listening is a network error, not a crash");
    }

    {
        HttpRequest request;
        request.url = base + "/slow";
        request.body = "{}";
        CancelToken stop;
        std::thread stopper([&] { std::this_thread::sleep_for(std::chrono::milliseconds(200)); stop.cancel(); });
        const auto started = std::chrono::steady_clock::now();
        auto response = client.post(request, stop);
        const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        stopper.join();
        check(response.cancelled && seconds < 1.5, "a cancel returns promptly while the server is still thinking");
        std::this_thread::sleep_for(std::chrono::milliseconds(2600));   // let the slow reply finish before the server closes
    }

    std::cout << (failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
