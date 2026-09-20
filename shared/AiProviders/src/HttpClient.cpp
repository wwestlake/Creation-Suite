#include "creation/ai/HttpClient.h"

#include <juce_core/juce_core.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace creation::ai
{
namespace
{
struct Shared
{
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;
    HttpResponse response;
};

HttpResponse performRequest(const HttpRequest& request)
{
    HttpResponse response;

    juce::String headers;
    for (const auto& [name, value] : request.headers)
        headers << juce::String(name) << ": " << juce::String(value) << "\r\n";

    int status = 0;
    juce::StringPairArray responseHeaders;
    auto url = juce::URL(juce::String(request.url)).withPOSTData(juce::String(request.body));
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs((int) (request.timeoutSeconds * 1000.0))
                                            .withStatusCode(&status)
                                            .withResponseHeaders(&responseHeaders)
                                            .withExtraHeaders(headers));

    if (stream == nullptr)
    {
        response.networkError = "Could not connect to " + request.url + ".";
        return response;
    }

    response.status = status;
    response.body = stream->readEntireStreamAsString().toStdString();
    response.retryAfter = responseHeaders.getValue("Retry-After", "").toStdString();
    return response;
}
}

HttpResponse JuceHttpClient::post(const HttpRequest& request, const CancelToken& cancel)
{
    auto shared = std::make_shared<Shared>();

    std::thread([shared, request]
    {
        auto result = performRequest(request);
        std::lock_guard<std::mutex> lock(shared->mutex);
        shared->response = std::move(result);
        shared->done = true;
        shared->finished.notify_all();
    }).detach();

    std::unique_lock<std::mutex> lock(shared->mutex);
    while (! shared->done)
    {
        shared->finished.wait_for(lock, std::chrono::milliseconds(20));
        if (! shared->done && cancel.isCancelled())
        {
            HttpResponse cancelled;
            cancelled.cancelled = true;
            cancelled.networkError = "Cancelled.";
            return cancelled;
        }
    }
    return shared->response;
}
}
