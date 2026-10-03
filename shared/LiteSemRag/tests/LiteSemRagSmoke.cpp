#include <creation/litesemrag/CardStore.h>
#include <creation/litesemrag/Retrieval.h>
#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>

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
