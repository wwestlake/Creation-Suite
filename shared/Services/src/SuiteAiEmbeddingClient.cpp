#include <creation/services/SuiteAiEmbeddingClient.h>
#include <creation/services/SuiteAiProviderRuntime.h>

namespace creation::services
{
namespace
{
constexpr int textsPerCall = 64;

// OpenAI's text-embedding-3 models can return shorter vectors; 512 numbers match nearly as well as 1536 and keep the
// stored vectors small.
constexpr int openAiDimensions = 512;

bool readVector(const juce::var& value, std::vector<float>& out)
{
    const auto* numbers = value.getArray();
    if (numbers == nullptr || numbers->isEmpty())
        return false;
    out.clear();
    out.reserve(static_cast<size_t>(numbers->size()));
    for (const auto& number : *numbers)
        out.push_back(static_cast<float>(static_cast<double>(number)));
    return true;
}

// One call: these texts, in order.
bool embedBatch(const SuiteAiResolvedRuntimeSettings& settings, const SuiteAiProviderRuntimeProfile& profile,
                const juce::String& model, const juce::StringArray& texts, SuiteAiEmbeddingClient::Result& result)
{
    juce::Array<juce::var> input;
    for (const auto& text : texts)
        input.add(text);

    auto* root = new juce::DynamicObject();
    root->setProperty("model", model);
    root->setProperty("input", juce::var(input));
    if (model.startsWith("text-embedding-3"))
        root->setProperty("dimensions", openAiDimensions);

    auto url = juce::URL(SuiteAiProviderRuntime::normalizeBaseUrl(settings.baseUrl, profile) + "/embeddings")
                   .withPOSTData(juce::JSON::toString(juce::var(root), true));
    int statusCode = 0;
    auto stream = url.createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                                            .withHttpRequestCmd("POST")
                                            .withConnectionTimeoutMs(30000)
                                            .withStatusCode(&statusCode)
                                            .withExtraHeaders(SuiteAiProviderRuntime::buildAuthHeaders(profile, settings.apiKey)));
    result.statusCode = statusCode;
    if (stream == nullptr)
    {
        result.errorMessage = "Could not connect to " + profile.displayName + " for embeddings.";
        return false;
    }
    const auto raw = stream->readEntireStreamAsString();
    if (statusCode < 200 || statusCode >= 300)
    {
        result.errorMessage = profile.displayName + " embeddings error (HTTP " + juce::String(statusCode) + "): " + raw.substring(0, 300);
        return false;
    }

    const auto parsed = juce::JSON::parse(raw);
    std::vector<std::vector<float>> vectors(static_cast<size_t>(texts.size()));
    const auto* data = parsed.getProperty("data", {}).getArray();
    if (data == nullptr || data->size() != texts.size())
    {
        result.errorMessage = profile.displayName + " returned the wrong number of embeddings.";
        return false;
    }
    for (int i = 0; i < data->size(); ++i)
    {
        const auto& item = data->getReference(i);
        const int index = item.hasProperty("index") ? static_cast<int>(item.getProperty("index", i)) : i;
        if (index < 0 || index >= texts.size() || ! readVector(item.getProperty("embedding", {}), vectors[static_cast<size_t>(index)]))
        {
            result.errorMessage = profile.displayName + " returned a malformed embedding.";
            return false;
        }
    }

    for (auto& vector : vectors)
        result.vectors.push_back(std::move(vector));
    return true;
}
} // namespace

SuiteAiEmbeddingClient::EmbeddingModel SuiteAiEmbeddingClient::embeddingModelFor(const SuiteAiResolvedRuntimeSettings& settings)
{
    // text-embedding-3-small at 512 numbers, measured with creation_suite_meaning_live_check (2026-10-03): a request's
    // right card scored 0.21-0.42 and was always the closest; requests no card was about (weather, a poem) stayed at
    // 0.13 or below; a vague request ("hold my company details in one value") put two cards within 0.05 of each other.
    if (SuiteAiProviderRuntime::normalizeProviderId(settings.providerId) == "openai")
        return { "text-embedding-3-small", 0.18f, 0.08f };
    return {};
}

bool SuiteAiEmbeddingClient::embed(const SuiteAiResolvedRuntimeSettings& settings, const juce::StringArray& texts, Result& result) const
{
    result = {};
    const auto profile = SuiteAiProviderRuntime::resolveProfile(settings.providerId);
    result.model = embeddingModelFor(settings).name;
    if (result.model.isEmpty())
    {
        result.errorMessage = profile.displayName + " has no embedding model the suite has measured.";
        return false;
    }
    if (SuiteAiProviderRuntime::requiresApiKey(profile, settings.apiKey))
    {
        result.errorMessage = "The suite's AI account for this app has no key.";
        return false;
    }

    for (int start = 0; start < texts.size(); start += textsPerCall)
    {
        juce::StringArray batch;
        for (int i = start; i < juce::jmin(texts.size(), start + textsPerCall); ++i)
            batch.add(texts[i]);
        if (! embedBatch(settings, profile, result.model, batch, result))
        {
            result.vectors.clear();
            return false;
        }
    }
    return true;
}
} // namespace creation::services
