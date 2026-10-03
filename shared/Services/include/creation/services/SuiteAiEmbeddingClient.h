#pragma once

#include <creation/services/SuiteAiSettings.h>

#include <vector>

namespace creation::services
{
// Turns text into embeddings - vectors that sit close together when the texts mean the same thing - through the same
// suite AI account as chat (SuiteAiChatClient). LiteSemRAG uses them to match a request to cards by meaning, not only
// by shared words.
//
// Not every provider has an embedding API (Anthropic has none), and how close "close" is differs from model to model.
// So the suite uses a provider's embeddings only once its model has been measured with
// creation_suite_meaning_live_check (shared/VirtualEngineer/tests/MeaningLiveCheck.cpp); embeddingModelFor is empty
// for any other, and the caller then matches on words alone.
class SuiteAiEmbeddingClient final
{
public:
    struct EmbeddingModel
    {
        juce::String name; // empty: none the suite uses
        // A card applies by meaning at `floor` or above and within `margin` of the closest card (LiteSemRAG MeaningMatch).
        float floor = 0.0f;
        float margin = 0.0f;
    };

    struct Result
    {
        std::vector<std::vector<float>> vectors; // one per text, in order
        juce::String model;
        juce::String errorMessage;
        int statusCode = 0;
    };

    static EmbeddingModel embeddingModelFor(const SuiteAiResolvedRuntimeSettings& settings);

    // All the texts in as few calls as the provider allows. False with errorMessage set on any failure; then no vectors.
    bool embed(const SuiteAiResolvedRuntimeSettings& settings, const juce::StringArray& texts, Result& result) const;
};
} // namespace creation::services
