#pragma once

#include <creation/ai/Provider.h>

#include <memory>
#include <string>
#include <vector>

namespace creation::ai
{
struct HttpRequest
{
    std::string url;
    std::vector<std::pair<std::string, std::string>> headers;
    std::string body;                 // POSTed as-is
    double timeoutSeconds = 120.0;    // connecting, and waiting for the reply
};

struct HttpResponse
{
    int status = 0;                   // 0 when there was no reply at all
    std::string body;
    std::string retryAfter;           // the Retry-After header, if any
    std::string networkError;         // set when there was no reply (could not connect, timed out, cancelled)
    bool cancelled = false;
};

// The one thing a provider needs from the network: POST some JSON and get the reply. Behind an interface so
// providers are tested without a network and a host can supply its own transport.
class HttpClient
{
public:
    virtual ~HttpClient() = default;

    // Blocks. Honours the cancel token: a cancelled request returns promptly with `cancelled` set, even
    // while the server has not answered.
    virtual HttpResponse post(const HttpRequest& request, const CancelToken& cancel) = 0;
};

// The real transport, on JUCE's networking. Each request runs on a helper thread that this call waits on, so a
// cancel returns at once; the helper finishes by itself when the server answers or the timeout passes.
class JuceHttpClient final : public HttpClient
{
public:
    HttpResponse post(const HttpRequest& request, const CancelToken& cancel) override;
};
}
