#pragma once

#include <creation/ai/HttpClient.h>
#include <creation/ai/Messages.h>

#include <string>

// The OpenAI chat-completions wire format, used by OpenAI and by the many providers that copy it (xAI,
// OpenRouter, Groq, Fireworks, DeepSeek, Together, Azure OpenAI, LM Studio, and custom OpenAI-compatible
// servers). Converting to and from it is pure functions on strings, so it is tested without a network.
namespace creation::ai::openai
{
struct WireOptions
{
    // OpenAI's current models take `max_completion_tokens`; many compatible servers only know `max_tokens`.
    bool useMaxCompletionTokens = false;
    // Some reasoning models accept only the default temperature; leave it out for them.
    bool sendTemperature = true;
};

// The JSON body for POST {base}/chat/completions.
std::string buildRequestBody(const ChatRequest& request, const WireOptions& options);

// Turns the provider's reply (or error) into a ChatResponse. Never throws; a reply that cannot be read is an
// error response saying why.
ChatResponse parseResponse(const HttpResponse& http);
}
