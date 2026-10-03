#pragma once

#include <creation/litesemrag/Cards.h>

#include <functional>
#include <vector>

namespace creation::litesemrag
{
// One scope's cards, as handed to retrieval.
struct ScopedCards
{
    CardScope scope = CardScope::suite;
    juce::Array<Card> cards;
};

struct RetrievedCard
{
    Card card;
    CardScope scope = CardScope::suite;
    juce::StringArray wordsMatched; // the query's words found in the card
    bool tokenMatched = false;      // one of them is in the card's own tokens
    float meaning = -1.0f;          // similarity of meaning to the query, 0..1; -1 when matched on words alone
};

struct Retrieval
{
    juce::String query;
    juce::StringArray tokens;          // what the query was matched on
    std::vector<RetrievedCard> cards;  // in the order they go into the prompt
    juce::String context;              // the text for the prompt; empty when nothing applies
    bool byMeaning = false;            // false: words alone
    juce::String wordsOnlyBecause;     // why, when meaning was wanted but not available
};

// Matching by meaning for one query (Embeddings.h): the query's embedding, a card's (null when it has none) and how
// close in meaning a card must be to apply - at least `floor`, and within `margin` of the closest card. How close
// "close" is depends on the embedding model; the values come with it (SuiteAiEmbeddingClient::EmbeddingModel).
struct MeaningMatch
{
    std::vector<float> query;
    std::function<const std::vector<float>*(const Card&)> cardVector;
    float floor = 0.18f;
    float margin = 0.08f;
};

// The words a query is matched on (research LiteSemRAG): lower case, split on spaces and punctuation, only letters,
// digits and _, longer than two characters, no common filler words, each once, at most 12.
juce::StringArray queryTokens(const juce::String& query);

// The cards that apply to a query. Across scopes a card id is one card: the closest scope's version wins (project, then
// app, then suite, then shipped), so a project can sharpen or retire a suite rule. Of those, an active card applies when
// it is close enough in meaning (with `meaning`: see MeaningMatch), or when one of the query's words starts a word of
// its id, title, kind, text or tokens ("struct" finds "structs", not "construct"); personality cards always do. Higher
// priority first, then the closer match - a card's own token first, then meaning, plus a little for each other word
// found - then the closer scope; at most maxCards; each card's text cut at 1400 characters.
Retrieval retrieve(const juce::String& query, const std::vector<ScopedCards>& scopes, int maxCards = 6,
                   const MeaningMatch* meaning = nullptr);
} // namespace creation::litesemrag
