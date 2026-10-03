#include <creation/litesemrag/Retrieval.h>

#include <creation/litesemrag/Embeddings.h>

#include <algorithm>
#include <map>
#include <set>

namespace creation::litesemrag
{
namespace
{
juce::String trimForPrompt(juce::String content)
{
    content = content.trim();
    if (content.length() <= 1400)
        return content;
    return content.substring(0, 1400).trim() + "\n...";
}

bool isWordCharacter(juce::juce_wchar c)
{
    return juce::CharacterFunctions::isLetterOrDigit(c) || c == '_';
}

// The query words that start a word of the text: "struct" finds "structs" and "struct_name", not "construct".
juce::StringArray wordsFound(const juce::String& text, const juce::StringArray& tokens)
{
    const auto lower = text.toLowerCase();
    juce::StringArray found;
    for (const auto& token : tokens)
        for (int at = lower.indexOf(token); at >= 0; at = lower.indexOf(at + 1, token))
            if (at == 0 || ! isWordCharacter(lower[at - 1]))
            {
                found.add(token);
                break;
            }
    return found;
}

// How closely a card matches, for ordering cards of the same priority. One of the card's own tokens outranks any
// closeness of meaning: a token is a word the card was written to answer to, and an exact name - a keyword, a node,
// an error code - must beat a near meaning (docs/architecture/Suite-Agent-Runtime-Spec.md). Then meaning, plus a little
// for each other word found.
float closeness(const RetrievedCard& retrieved)
{
    return (retrieved.tokenMatched ? 2.0f : 0.0f) + juce::jmax(0.0f, retrieved.meaning)
           + 0.1f * static_cast<float>(juce::jmin(3, retrieved.wordsMatched.size()));
}

void appendList(juce::String& out, const char* label, const juce::StringArray& items)
{
    if (items.isEmpty())
        return;
    out << label << ":\n";
    for (int i = 0; i < items.size(); ++i)
        out << "  " << (i + 1) << ". " << items[i] << "\n";
}
} // namespace

juce::StringArray queryTokens(const juce::String& query)
{
    juce::StringArray raw;
    raw.addTokens(query.toLowerCase(), " \t\r\n.,!?;:()[]{}<>+-=*/\\|&^%\"'", "");

    static const std::set<juce::String> stopWords {
        "about", "after", "also", "and", "are", "build", "can", "code", "does", "for", "from", "have", "how", "into",
        "like", "make", "need", "the", "this", "that", "use", "what", "when", "where", "with", "write"
    };

    juce::StringArray tokens;
    for (auto token : raw)
    {
        token = token.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789_");
        if (token.length() <= 2 || stopWords.count(token) != 0 || tokens.contains(token))
            continue;
        tokens.add(token);
        if (tokens.size() >= 12)
            break;
    }
    return tokens;
}

Retrieval retrieve(const juce::String& query, const std::vector<ScopedCards>& scopes, int maxCards, const MeaningMatch* meaning)
{
    Retrieval result;
    result.query = query;
    result.tokens = queryTokens(query);

    // One card per id: the closest scope's version.
    std::map<juce::String, RetrievedCard> byId;
    std::vector<juce::String> order; // first appearance, for a stable result
    for (const auto& scoped : scopes)
        for (const auto& card : scoped.cards)
        {
            if (card.id.isEmpty())
                continue;
            auto found = byId.find(card.id);
            if (found == byId.end())
            {
                byId[card.id] = { card, scoped.scope };
                order.push_back(card.id);
            }
            else if (static_cast<int>(scoped.scope) > static_cast<int>(found->second.scope))
                found->second = { card, scoped.scope };
        }

    result.byMeaning = meaning != nullptr && ! meaning->query.empty();
    std::vector<RetrievedCard> candidates;
    float closest = 0.0f; // the closest meaning of any card a request could bring up
    for (const auto& id : order)
    {
        auto candidate = byId[id];
        if (! candidate.card.isActive() || candidate.card.text.trim().isEmpty())
            continue;
        juce::String words;
        words << candidate.card.id << " " << candidate.card.title << " " << candidate.card.kind << " " << candidate.card.text << " "
              << candidate.card.tokens.joinIntoString(" ");
        candidate.wordsMatched = wordsFound(words, result.tokens);
        candidate.tokenMatched = ! wordsFound(candidate.card.tokens.joinIntoString(" "), result.tokens).isEmpty();
        if (result.byMeaning && meaning->cardVector)
            if (const auto* vector = meaning->cardVector(candidate.card))
                candidate.meaning = juce::jmax(0.0f, similarity(meaning->query, *vector));
        if (! candidate.card.kind.equalsIgnoreCase("personality"))
            closest = juce::jmax(closest, candidate.meaning);
        candidates.push_back(candidate);
    }

    std::vector<RetrievedCard> applying;
    for (const auto& candidate : candidates)
        if (candidate.card.kind.equalsIgnoreCase("personality") || ! candidate.wordsMatched.isEmpty()
            || (result.byMeaning && candidate.meaning >= meaning->floor && candidate.meaning >= closest - meaning->margin))
            applying.push_back(candidate);
    std::stable_sort(applying.begin(), applying.end(), [](const RetrievedCard& a, const RetrievedCard& b) {
        if (a.card.priority != b.card.priority)
            return a.card.priority > b.card.priority;
        if (const auto ca = closeness(a), cb = closeness(b); ca != cb)
            return ca > cb;
        return static_cast<int>(a.scope) > static_cast<int>(b.scope);
    });
    if (maxCards >= 0 && static_cast<int>(applying.size()) > maxCards)
        applying.resize(static_cast<size_t>(maxCards));
    result.cards = applying;

    for (const auto& retrieved : result.cards)
    {
        const auto& card = retrieved.card;
        if (result.context.isEmpty())
            result.context = "Guidance that applies to this request (LiteSemRAG cards). Follow it; process cards are binding.\n";
        result.context << "\n--- [" << scopeName(retrieved.scope) << " " << (card.kind.isEmpty() ? juce::String("card") : card.kind) << "] "
                       << card.title << " (" << card.id << ") ---\n"
                       << trimForPrompt(card.text) << "\n";
        if (card.trigger.isNotEmpty())
            result.context << "When: " << card.trigger << "\n";
        appendList(result.context, "Steps", card.steps);
        appendList(result.context, "Never", card.gates);
        appendList(result.context, "Evidence that it was followed", card.evidence);
        appendList(result.context, "Stop and ask when", card.escalation);
    }
    return result;
}
} // namespace creation::litesemrag
