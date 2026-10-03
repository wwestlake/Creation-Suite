#include <creation/litesemrag/CardStore.h>
#include <creation/litesemrag/Embeddings.h>
#include <creation/litesemrag/Retrieval.h>
#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace ls = creation::litesemrag;

namespace
{
int failures = 0;

void check(const char* what, bool good)
{
    if (good)
        std::cout << "ok   " << what << "\n";
    else
    {
        std::cerr << "FAIL " << what << "\n";
        ++failures;
    }
}

ls::Card card(const char* id, const char* kind, const char* title, const char* text, juce::StringArray tokens, int priority)
{
    ls::Card c;
    c.id = id;
    c.kind = kind;
    c.title = title;
    c.text = text;
    c.tokens = tokens;
    c.priority = priority;
    c.source = "user";
    return c;
}
} // namespace

int main()
{
    // Tokens: "How do I write a Frust struct with an enum field?" -> frust, struct, enum, field ("how" "write" "with"
    // are filler, "do" "i" "a" "an" too short).
    check("Query tokens drop filler and short words",
          ls::queryTokens("How do I write a Frust struct with an enum field?") == juce::StringArray { "frust", "struct", "enum", "field" });

    // Cards in every scope. The project retires the suite's storage rule and replaces its Structs card.
    auto storage = card("suite.rule.vfs", "rule", "Storage", "Nothing but the root pointer goes on the OS.", { "vfs", "storage", "appdata" }, 90);
    auto structs = card("suite.knowledge.structs", "knowledge", "Structs", "A struct is a product type.", { "struct" }, 40);
    auto personality = card("suite.personality", "personality", "Voice", "Answer plainly.", {}, 10);
    auto nodes = card("texture.process.nodes", "process", "Adding a node", "Add nodes through the palette.", { "node", "graph" }, 60);
    nodes.steps = { "Read the graph", "Add the node" };
    nodes.gates = { "Never rename a node" };
    auto retiredStorage = storage;
    retiredStorage.status = "retired";
    auto projectStructs = structs;
    projectStructs.text = "Project structs use Pascal case.";

    const std::vector<ls::ScopedCards> scopes {
        { ls::CardScope::suite, { storage, structs, personality } },
        { ls::CardScope::app, { nodes } },
        { ls::CardScope::project, { retiredStorage, projectStructs } },
    };

    // "add a struct node to the graph and keep storage in the vfs": the app's process card (60), the project's Structs
    // card (40, replacing the suite's), the personality card (10); the storage rule is retired by the project.
    const auto found = ls::retrieve("add a struct node to the graph and keep storage in the vfs", scopes);
    check("Three cards apply, process card first, personality last",
          found.cards.size() == 3 && found.cards[0].card.id == "texture.process.nodes" && found.cards[1].card.id == "suite.knowledge.structs"
              && found.cards[2].card.id == "suite.personality");
    check("The project's version of a card replaces the suite's",
          found.cards.size() == 3 && found.cards[1].scope == ls::CardScope::project
              && found.context.contains("Project structs use Pascal case.") && ! found.context.contains("A struct is a product type."));
    check("A card retired in the project hides the suite's", ! found.context.contains("Nothing but the root pointer"));
    check("A process card brings its steps and gates",
          found.context.contains("Steps:\n  1. Read the graph\n  2. Add the node\n") && found.context.contains("Never:\n  1. Never rename a node\n"));
    check("At most maxCards", ls::retrieve("add a struct node to the graph", scopes, 1).cards.size() == 1);
    const auto nothing = ls::retrieve("hello there", scopes);
    check("Nothing matching: only the personality card", nothing.cards.size() == 1 && nothing.cards[0].card.id == "suite.personality");
    // A word matches where it starts a word of the card: "constructor" is not "struct".
    const auto inWord = ls::retrieve("constructor", { { ls::CardScope::suite, { structs } } });
    check("A word inside another word does not match", inWord.cards.empty() && ! inWord.byMeaning);
    const auto prefix = ls::retrieve("node", { { ls::CardScope::app, { nodes } } });
    check("A word that starts a card's word matches, and says which",
          prefix.cards.size() == 1 && prefix.cards[0].wordsMatched == juce::StringArray { "node" } && prefix.cards[0].meaning < 0.0f);

    // Meaning. Hand-made vectors stand in for embeddings: the query points along x; Structs nearly along it
    // (similarity 0.9 / sqrt(0.82) = 0.994), Adding a node a little (0.3 / sqrt(0.9925) = 0.301, under 0.35), Storage
    // not at all (0).
    // "how do I keep a record of fields" shares no card word (keep, record, fields), so only meaning can find Structs.
    ls::MeaningMatch meaning;
    meaning.query = { 1.0f, 0.0f, 0.0f };
    meaning.cardVector = [](const ls::Card& c) -> const std::vector<float>* {
        static const std::vector<float> structsVector { 0.9f, 0.1f, 0.0f }, nodesVector { 0.3f, 0.95f, 0.0f }, storageVector { 0.0f, 1.0f, 0.0f };
        if (c.id == "suite.knowledge.structs") return &structsVector;
        if (c.id == "texture.process.nodes") return &nodesVector;
        if (c.id == "suite.rule.vfs") return &storageVector;
        return nullptr;
    };
    const std::vector<ls::ScopedCards> plain { { ls::CardScope::suite, { storage, structs } }, { ls::CardScope::app, { nodes } } };
    const auto byMeaning = ls::retrieve("how do I keep a record of fields", plain, 6, &meaning);
    check("Meaning finds a card that shares no word with the request",
          byMeaning.byMeaning && byMeaning.cards.size() == 1 && byMeaning.cards[0].card.id == "suite.knowledge.structs"
              && byMeaning.cards[0].meaning > 0.99f && byMeaning.cards[0].wordsMatched.isEmpty());
    check("Without meaning the same request finds nothing", ls::retrieve("how do I keep a record of fields", plain).cards.empty());
    // Same priority: the closer meaning goes first. Both 50; Structs 0.994, Adding a node 0.301 plus 0.1 for "palette" (a
    // word of its text, not one of its tokens).
    auto structs50 = structs;
    structs50.priority = 50;
    auto nodes50 = nodes;
    nodes50.priority = 50;
    const auto ranked = ls::retrieve("the palette", { { ls::CardScope::suite, { nodes50, structs50 } } }, 6, &meaning);
    check("Of the same priority, the closer match goes first",
          ranked.cards.size() == 2 && ranked.cards[0].card.id == "suite.knowledge.structs" && ranked.cards[1].card.id == "texture.process.nodes");

    // An exact name beats a near meaning: the error card answers to its token "e0042" and has no embedding; Structs is
    // 0.994 close in meaning. Both priority 50.
    const auto errorCard = card("suite.knowledge.e0042", "knowledge", "Error E0042", "E0042 means a type was used before it was declared.", { "e0042" }, 50);
    const auto exact = ls::retrieve("what does e0042 mean for a record", { { ls::CardScope::suite, { structs50, errorCard } } }, 6, &meaning);
    check("A card's own token outranks a closer meaning",
          exact.cards.size() == 2 && exact.cards[0].card.id == "suite.knowledge.e0042" && exact.cards[0].tokenMatched
              && exact.cards[1].card.id == "suite.knowledge.structs" && ! exact.cards[1].tokenMatched);

    check("Similarity: same direction 1, at right angles 0, different lengths 0",
          std::abs(ls::similarity({ 2.0f, 0.0f }, { 1.0f, 0.0f }) - 1.0f) < 1.0e-6f && ls::similarity({ 1.0f, 0.0f }, { 0.0f, 3.0f }) == 0.0f
              && ls::similarity({ 1.0f }, { 1.0f, 0.0f }) == 0.0f);
    check("An embedding key follows the card's words and the model",
          ls::embeddingKey(structs, "m") == ls::embeddingKey(structs, "m") && ls::embeddingKey(structs, "m") != ls::embeddingKey(projectStructs, "m")
              && ls::embeddingKey(structs, "m") != ls::embeddingKey(structs, "other"));

    // Validation and the stored form.
    check("A card needs an id without spaces, a title and text, and a unique id",
          ls::validateCard(card("", "rule", "T", "x", {}, 50), {}).isNotEmpty()
              && ls::validateCard(card("a b", "rule", "T", "x", {}, 50), {}).isNotEmpty()
              && ls::validateCard(card("a", "rule", "", "x", {}, 50), {}).isNotEmpty()
              && ls::validateCard(card("a", "rule", "T", "x", {}, 50), { card("a", "rule", "T", "y", {}, 50) }).isNotEmpty()
              && ls::validateCard(card("a", "rule", "T", "x", {}, 50), {}).isEmpty());
    const auto back = ls::cardsFromVar(juce::JSON::parse(juce::JSON::toString(ls::cardsToVar({ nodes, structs }))));
    check("Cards round-trip through their stored form", back.size() == 2 && back[0] == nodes && back[1] == structs);

    // The store, in the VFS - in this test's own scope, never the real cards.
    creation::services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        std::cerr << "FAIL the suite VFS service is not reachable\n";
        return 1;
    }
    juce::MemoryBlock realSuiteCards;
    const bool hadRealSuiteCards = client.readEntry("cards/suite.json", realSuiteCards);
    juce::String error;
    creation::services::SuiteVfsJsonStore::setScopeForTesting("litesemrag-smoke");
    creation::services::SuiteVfsJsonStore::removeScopeForTesting(error);

    error.clear();
    const bool saved = ls::CardStore::save(ls::CardStore::suite(), { storage, structs }, error);
    const auto loaded = ls::CardStore::load(ls::CardStore::suite(), error);
    check("Suite cards are saved to and read back from the VFS", saved && loaded.size() == 2 && loaded[0] == storage);

    error.clear();
    const auto app = ls::CardStore::forApp(creation::assets::SuiteAppDomain::texture);
    const bool appSaved = ls::CardStore::upsert(app, nodes, error);
    auto changed = nodes;
    changed.text = "Add nodes with Enter in the palette.";
    ls::CardStore::upsert(app, changed, error);
    const auto appCards = ls::CardStore::load(app, error);
    check("App cards: upsert adds, then replaces by id", appSaved && appCards.size() == 1 && appCards[0].text == changed.text);
    check("Removing a card", ls::CardStore::remove(app, nodes.id, error) && ls::CardStore::load(app, error).isEmpty());
    error.clear();
    check("An invalid card is refused, with the reason", ! ls::CardStore::upsert(app, card("bad id", "rule", "T", "x", {}, 50), error)
                                                            && error.contains("no spaces"));
    error.clear();
    check("Shipped cards cannot be written", ! ls::CardStore::save({ ls::CardScope::shipped }, { storage }, error) && error.contains("built into"));

    // The embedding cache: saved to the VFS and read back.
    error.clear();
    auto cache = ls::EmbeddingCache::load("test-model", error);
    cache.put("k1", { 0.25f, -0.5f, 0.125f });
    const bool cacheSaved = cache.save(error);
    const auto reread = ls::EmbeddingCache::load("test-model", error);
    const auto* vector = reread.find("k1");
    check("Embeddings are kept in the VFS and read back",
          cacheSaved && ! cache.hasChanges() && vector != nullptr && *vector == std::vector<float> { 0.25f, -0.5f, 0.125f }
              && reread.find("k2") == nullptr && error.isEmpty());

    // Project cards live inside the project.
    juce::String projectId, createError;
    creation::assets::ProjectManifest manifest;
    const bool created = client.createProject(creation::assets::SuiteAppDomain::texture, "LiteSemRag Smoke Test Project", "0.0.0-smoke",
                                              "0.0.0-smoke", projectId, manifest, createError);
    error.clear();
    const auto project = ls::CardStore::forProject(projectId);
    const bool projectSaved = created && ls::CardStore::upsert(project, projectStructs, error);
    const auto projectCards = ls::CardStore::load(project, error);
    juce::MemoryBlock inside;
    check("Project cards are stored inside the project",
          projectSaved && projectCards.size() == 1 && projectCards[0] == projectStructs
              && client.readProjectEntry(projectId, "Cards/cards.json", inside));
    juce::String deleteError;
    if (created)
        client.deleteProject(projectId, deleteError);

    // Leave nothing behind; the real suite cards were never touched.
    creation::services::SuiteVfsJsonStore::removeScopeForTesting(error);
    creation::services::SuiteVfsJsonStore::setScopeForTesting({});
    juce::MemoryBlock afterSuiteCards;
    const bool hasRealSuiteCards = client.readEntry("cards/suite.json", afterSuiteCards);
    check("The real suite cards were not touched", hasRealSuiteCards == hadRealSuiteCards && afterSuiteCards == realSuiteCards);

    std::cout << (failures == 0 ? "LiteSemRag: ok" : "LiteSemRag: FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
