#pragma once

#include <creation/litesemrag/Cards.h>

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
};

struct Retrieval
{
    juce::String query;
    juce::StringArray tokens;          // what the query was matched on
    std::vector<RetrievedCard> cards;  // in the order they go into the prompt
    juce::String context;              // the text for the prompt; empty when nothing applies
};

// The words a query is matched on (research LiteSemRAG): lower case, split on spaces and punctuation, only letters,
// digits and _, longer than two characters, no common filler words, each once, at most 12.
juce::StringArray queryTokens(const juce::String& query);

// The cards that apply to a query. Across scopes a card id is one card: the closest scope's version wins (project, then
// app, then suite, then shipped), so a project can sharpen or retire a suite rule. Of those, the active ones that match
// a token (in id, title, kind, text or tokens) apply, and personality cards always do. Higher priority first, then the
// closer scope; at most maxCards; each card's text cut at 1400 characters.
Retrieval retrieve(const juce::String& query, const std::vector<ScopedCards>& scopes, int maxCards = 6);
} // namespace creation::litesemrag
