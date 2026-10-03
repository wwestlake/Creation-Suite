#pragma once

#include <creation/litesemrag/Cards.h>

#include <map>
#include <vector>

namespace creation::litesemrag
{
// Matching by meaning. A card's embedding is made once from the card's words (embeddingText) and kept; a request is
// embedded when it is made, and how close the two vectors point (similarity) says how near in meaning they are. The
// embeddings themselves come from the suite's AI account (creation::services::SuiteAiEmbeddingClient); nothing here
// talks to a provider.

// What a card is embedded from: its title, kind, trigger, tokens and text.
juce::String embeddingText(const Card& card);

// Names one embedding: the model and exactly the words it was made from. A changed card gets a new key, so its old
// embedding is never used for it again.
juce::String embeddingKey(const Card& card, const juce::String& model);

// Cosine similarity, -1..1; 0 for vectors of different lengths or no length.
float similarity(const std::vector<float>& a, const std::vector<float>& b);

// The embeddings made so far, for one model: the suite entry cards/embeddings/<model>.json in the VFS, shared by every
// app and project (a key is the card's words, so the same card in two places is embedded once). It is a cache: losing
// an entry only means making it again. Two apps saving at once may drop each other's newest entries for the same reason.
class EmbeddingCache final
{
public:
    static EmbeddingCache load(const juce::String& model, juce::String& errorMessage);

    const juce::String& getModel() const noexcept { return model; }
    const std::vector<float>* find(const juce::String& key) const;
    void put(const juce::String& key, std::vector<float> vector);
    bool hasChanges() const noexcept { return changed; }

    // Saves when something was added; keeps the most recently made entries up to the limit.
    bool save(juce::String& errorMessage);

    static constexpr int maxEntries = 4000;

private:
    struct Entry
    {
        std::vector<float> vector;
        juce::int64 madeAt = 0;
    };

    juce::String model;
    std::map<juce::String, Entry> entries;
    bool changed = false;
};
} // namespace creation::litesemrag
