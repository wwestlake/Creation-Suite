#include <creation/litesemrag/Retrieval.h>

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

bool matches(const Card& card, const juce::StringArray& tokens)
{
    if (card.kind.equalsIgnoreCase("personality"))
        return true;
    juce::String haystack;
    haystack << card.id << " " << card.title << " " << card.kind << " " << card.text;
    for (const auto& token : card.tokens)
        haystack << " " << token;
    const auto lower = haystack.toLowerCase();
    for (const auto& token : tokens)
        if (lower.contains(token))
            return true;
    return false;
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

Retrieval retrieve(const juce::String& query, const std::vector<ScopedCards>& scopes, int maxCards)
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

    std::vector<RetrievedCard> applying;
    for (const auto& id : order)
    {
        const auto& candidate = byId[id];
        if (candidate.card.isActive() && candidate.card.text.trim().isNotEmpty()
            && (candidate.card.kind.equalsIgnoreCase("personality") || (! result.tokens.isEmpty() && matches(candidate.card, result.tokens))))
            applying.push_back(candidate);
    }
    std::stable_sort(applying.begin(), applying.end(), [](const RetrievedCard& a, const RetrievedCard& b) {
        if (a.card.priority != b.card.priority)
            return a.card.priority > b.card.priority;
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
