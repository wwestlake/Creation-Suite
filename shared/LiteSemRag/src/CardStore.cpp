#include <creation/litesemrag/CardStore.h>

#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>

namespace creation::litesemrag
{
namespace
{
constexpr const char* projectCardsEntry = "Cards/cards.json";

juce::String suiteEntryFor(const CardStore::Place& place)
{
    if (place.scope == CardScope::app)
        return "cards/app/" + creation::assets::toStorageToken(place.app) + ".json";
    return "cards/suite.json";
}

juce::String placeProblem(const CardStore::Place& place)
{
    if (place.scope == CardScope::shipped)
        return "Shipped cards are built into the suite and cannot be changed here.";
    if (place.scope == CardScope::app && place.app == creation::assets::SuiteAppDomain::unknown)
        return "App cards need the app they belong to.";
    if (place.scope == CardScope::project && place.projectId.isEmpty())
        return "Project cards need an open project.";
    return {};
}
} // namespace

CardStore::Place CardStore::suite()
{
    return { CardScope::suite, creation::assets::SuiteAppDomain::unknown, {} };
}

CardStore::Place CardStore::forApp(creation::assets::SuiteAppDomain app)
{
    return { CardScope::app, app, {} };
}

CardStore::Place CardStore::forProject(const juce::String& projectId)
{
    return { CardScope::project, creation::assets::SuiteAppDomain::unknown, projectId };
}

juce::Array<Card> CardStore::load(const Place& place, juce::String& errorMessage)
{
    if (const auto problem = placeProblem(place); problem.isNotEmpty())
    {
        errorMessage = problem;
        return {};
    }
    if (place.scope != CardScope::project)
        return cardsFromVar(creation::services::SuiteVfsJsonStore::loadJson(suiteEntryFor(place), errorMessage));

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return {};
    }
    juce::MemoryBlock data;
    if (! client.readProjectEntry(place.projectId, projectCardsEntry, data))
        return {}; // no project cards yet
    const auto parsed = juce::JSON::parse(data.toString());
    if (parsed.isVoid())
    {
        errorMessage = "Could not read the project's cards.";
        return {};
    }
    return cardsFromVar(parsed);
}

bool CardStore::save(const Place& place, const juce::Array<Card>& cards, juce::String& errorMessage)
{
    if (const auto problem = placeProblem(place); problem.isNotEmpty())
    {
        errorMessage = problem;
        return false;
    }
    for (int i = 0; i < cards.size(); ++i)
    {
        juce::Array<Card> others(cards);
        others.remove(i);
        if (const auto problem = validateCard(cards.getReference(i), others); problem.isNotEmpty())
        {
            errorMessage = problem;
            return false;
        }
    }
    const auto value = cardsToVar(cards);
    if (place.scope != CardScope::project)
        return creation::services::SuiteVfsJsonStore::saveJson(suiteEntryFor(place), value, errorMessage);

    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        errorMessage = "Could not reach the suite VFS service.";
        return false;
    }
    const auto json = juce::JSON::toString(value, true);
    if (! client.writeProjectEntry(place.projectId, projectCardsEntry, juce::MemoryBlock(json.toRawUTF8(), json.getNumBytesAsUTF8())))
    {
        errorMessage = "Could not save the project's cards: " + client.getLastWriteError();
        return false;
    }
    return true;
}

bool CardStore::upsert(const Place& place, const Card& card, juce::String& errorMessage)
{
    auto cards = load(place, errorMessage);
    if (errorMessage.isNotEmpty())
        return false;
    bool replaced = false;
    for (auto& existing : cards)
        if (existing.id == card.id)
        {
            existing = card;
            replaced = true;
        }
    if (! replaced)
        cards.add(card);
    return save(place, cards, errorMessage);
}

bool CardStore::remove(const Place& place, const juce::String& cardId, juce::String& errorMessage)
{
    auto cards = load(place, errorMessage);
    if (errorMessage.isNotEmpty())
        return false;
    const auto before = cards.size();
    cards.removeIf([&cardId](const Card& card) { return card.id == cardId; });
    if (cards.size() == before)
    {
        errorMessage = "There is no card \"" + cardId + "\" there.";
        return false;
    }
    return save(place, cards, errorMessage);
}
} // namespace creation::litesemrag
