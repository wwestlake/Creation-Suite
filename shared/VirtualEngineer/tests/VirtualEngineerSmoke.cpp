#include <creation/agent/AgentApi.h>
#include <creation/agent/VirtualEngineer.h>
#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>

#include <atomic>
#include <iostream>

namespace ls = creation::litesemrag;
namespace services = creation::services;

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

ls::Card card(const char* id, const char* title, const char* text, juce::StringArray tokens)
{
    ls::Card c;
    c.id = id;
    c.kind = "rule";
    c.title = title;
    c.text = text;
    c.tokens = tokens;
    return c;
}

bool waitUntilFree(const creation::agent::VirtualEngineer& engineer)
{
    for (int i = 0; i < 500 && engineer.isBusy(); ++i)
        juce::Thread::sleep(10);
    return ! engineer.isBusy();
}

// One HTTP call to the API: the status code and the parsed body.
juce::var call(const juce::String& baseUrl, const juce::String& path, const juce::String& token, int& status, const juce::String& postBody = {})
{
    auto url = juce::URL(baseUrl + path);
    if (postBody.isNotEmpty())
        url = url.withPOSTData(postBody);
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withConnectionTimeoutMs(5000)
                       .withStatusCode(&status)
                       .withExtraHeaders(token.isNotEmpty() ? "Authorization: Bearer " + token : juce::String());
    status = 0;
    auto stream = url.createInputStream(options);
    return stream != nullptr ? juce::JSON::parse(stream->readEntireStreamAsString()) : juce::var();
}
} // namespace

int main()
{
    services::SuiteVfsServiceClient client;
    if (! client.discover())
    {
        std::cerr << "FAIL the suite VFS service is not reachable\n";
        return 1;
    }
    juce::MemoryBlock realAiSettings;
    const bool hadRealAiSettings = client.readEntry("ai-settings.json", realAiSettings);

    // Everything below is in this test's own VFS scope: its AI account, its cards, its API announcement.
    juce::String error;
    services::SuiteVfsJsonStore::setScopeForTesting("virtual-engineer-smoke");
    services::SuiteVfsJsonStore::removeScopeForTesting(error);
    auto saveTestAccount = [] {
        services::SuiteAiSettings settings;
        settings.defaultAccountId = "test";
        settings.accounts.add({ "test", "openai", "Test account", "https://api.openai.com/v1", "gpt-test", "test-key", true });
        juce::String e;
        return services::SuiteAiSettingsStore().save(settings, e);
    };
    saveTestAccount();
    ls::CardStore::save(ls::CardStore::suite(), { card("suite.rule.vfs", "Storage", "Nothing but the root pointer goes on the OS.", { "storage" }) }, error);
    ls::CardStore::save(ls::CardStore::forApp(creation::assets::SuiteAppDomain::texture),
                        { card("texture.knowledge.graph", "Graphs", "Image graphs are typed image.", { "graph" }) }, error);

    creation::agent::VirtualEngineer engineer(creation::assets::SuiteAppDomain::texture);
    engineer.deliverDirectlyForTesting = true;
    engineer.appContext = [](const juce::String&) { return juce::String("Graph: 3 nodes"); };
    juce::String lastSystem, lastUser;
    int answers = 0;
    juce::WaitableEvent release(true);
    release.signal();
    engineer.transportForTesting = [&](const services::SuiteAiResolvedRuntimeSettings& settings, const juce::String& system,
                                       const juce::String& user, services::SuiteAiChatClient::ChatResult& result) {
        release.wait(5000);
        lastSystem = system;
        lastUser = user + "\nmodel=" + settings.modelName;
        result.text = "Answer " + juce::String(++answers);
        return true;
    };
    // Embeddings by hand: text about storage or files points along x, about graphs along y, anything else along z.
    std::atomic<int> embedCalls { 0 }, textsEmbedded { 0 };
    std::atomic<bool> embeddingsFail { false };
    engineer.embedderForTesting = [&](const services::SuiteAiResolvedRuntimeSettings&, const juce::StringArray& texts,
                                      services::SuiteAiEmbeddingClient::Result& result) {
        ++embedCalls;
        textsEmbedded += texts.size();
        if (embeddingsFail)
        {
            result.errorMessage = "test provider is down";
            return false;
        }
        for (const auto& text : texts)
        {
            const auto lower = text.toLowerCase();
            if (lower.contains("storage") || lower.contains("files") || lower.contains("root pointer"))
                result.vectors.push_back({ 1.0f, 0.0f, 0.0f });
            else if (lower.contains("graph"))
                result.vectors.push_back({ 0.0f, 1.0f, 0.0f });
            else
                result.vectors.push_back({ 0.0f, 0.0f, 1.0f });
        }
        return true;
    };

    // 1. The storage card and the app's context go in; the reply comes back.
    creation::agent::AskResult first;
    engineer.ask("Where does storage go?", [&](const creation::agent::AskResult& r) { first = r; });
    waitUntilFree(engineer);
    check("A request gets its reply", first.ok && first.text == "Answer 1");
    check("The matching card goes into the system prompt", lastSystem.contains("Nothing but the root pointer goes on the OS."));
    check("The app's context, the request and the suite account's model go out",
          lastUser.contains("Graph: 3 nodes") && lastUser.contains("Request:\nWhere does storage go?") && lastUser.contains("model=gpt-test"));
    check("The details say which cards were used", first.details.getProperty("cards", {})[0].getProperty("id", {}).toString() == "suite.rule.vfs");

    // 2. Another request: the app's graph card, not the storage card, and the earlier exchange.
    creation::agent::AskResult second;
    engineer.ask("And the graph?", [&](const creation::agent::AskResult& r) { second = r; });
    waitUntilFree(engineer);
    check("The next request brings its own cards",
          second.ok && lastSystem.contains("Image graphs are typed image.") && ! lastSystem.contains("Nothing but the root pointer"));
    check("The earlier exchange goes along", lastUser.contains("Earlier in this conversation:\nUser: Where does storage go?\nYou: Answer 1"));
    check("The details say the cards were matched by meaning", second.details.getProperty("matchedBy", {}).toString() == "meaning and words");

    // 2b. Meaning. "Where do my saved files live?" shares no word with the Storage card (saved, files, live), but means
    // the same; the cards were embedded by the first request, so now only the request itself is.
    embedCalls = 0;
    textsEmbedded = 0;
    error.clear();
    const auto byMeaning = engineer.retrieveFor("Where do my saved files live?", error);
    check("Meaning brings up a card that shares no word with the request",
          byMeaning.byMeaning && byMeaning.cards.size() == 1 && byMeaning.cards[0].card.id == "suite.rule.vfs"
              && byMeaning.cards[0].wordsMatched.isEmpty() && byMeaning.cards[0].meaning > 0.99f && error.isEmpty());
    check("A card is embedded once; after that only the request is", embedCalls == 1 && textsEmbedded == 1);
    ls::CardStore::upsert(ls::CardStore::suite(), card("suite.rule.files", "Files", "Project files are entries in the VFS.", {}), error);
    textsEmbedded = 0;
    engineer.retrieveFor("Where do my saved files live?", error);
    check("A new card is embedded the first time it could apply", textsEmbedded == 2);
    embeddingsFail = true;
    const auto wordsOnly = engineer.retrieveFor("Where does storage go?", error);
    check("When embeddings fail, cards are matched on words and the reason is given",
          ! wordsOnly.byMeaning && wordsOnly.cards.size() == 1 && wordsOnly.cards[0].card.id == "suite.rule.vfs"
              && wordsOnly.wordsOnlyBecause.contains("test provider is down"));
    embeddingsFail = false;
    ls::Retrieval later;
    std::atomic<bool> delivered { false };
    engineer.retrieveAsync("Where do my saved files live?", [&](const ls::Retrieval& r, const juce::String&) { later = r; delivered = true; });
    for (int i = 0; i < 500 && ! delivered; ++i)
        juce::Thread::sleep(10);
    check("The same off the message thread (the Cards panel's Try)", delivered && later.byMeaning && later.cards.size() == 2);
    ls::CardStore::remove(ls::CardStore::suite(), "suite.rule.files", error);

    // 3. One request at a time.
    release.reset();
    const bool started = engineer.ask("first", nullptr);
    const bool refused = ! engineer.ask("second", nullptr);
    release.signal();
    waitUntilFree(engineer);
    check("A second request while one runs is refused", started && refused);

    // 4. No AI account: say where to add one.
    services::SuiteVfsJsonStore::removeJson("ai-settings.json", error);
    creation::agent::AskResult noAccount;
    engineer.ask("Where does storage go?", [&](const creation::agent::AskResult& r) { noAccount = r; });
    waitUntilFree(engineer);
    check("Without an AI account the engineer says to add one in the suite's settings",
          ! noAccount.ok && noAccount.error.contains("No AI account") && noAccount.error.contains("suite's settings"));
    saveTestAccount();

    // 5. The API.
    creation::agent::AgentApi api(engineer);
    api.runDirectlyForTesting = true;
    api.addAppEndpoint("graph", "The open graph", [] {
        auto* graph = new juce::DynamicObject();
        graph->setProperty("nodes", 3);
        return juce::var(graph);
    });
    juce::String apiError;
    const bool apiStarted = api.start(apiError);
    const auto discovery = services::SuiteVfsJsonStore::loadJson(creation::agent::AgentApi::discoveryEntry(creation::assets::SuiteAppDomain::texture), error);
    const auto baseUrl = discovery.getProperty("baseUrl", {}).toString();
    const auto token = discovery.getProperty("token", {}).toString();
    check("The API announces itself in the VFS (agents/texture.json)", apiStarted && baseUrl.startsWith("http://127.0.0.1:") && token == api.getToken());

    int status = 0;
    call(baseUrl, "/v1/status", {}, status);
    check("No token: 401", status == 401);
    const auto info = call(baseUrl, "/v1/status", token, status);
    check("Status: the app and its endpoints",
          status == 200 && info.getProperty("app", {}).toString() == "texture" && info.getProperty("appEndpoints", {})[0].toString() == "graph");
    const auto graph = call(baseUrl, "/v1/app/graph", token, status);
    check("An app endpoint answers", status == 200 && static_cast<int>(graph.getProperty("nodes", 0)) == 3);
    const auto match = call(baseUrl, "/v1/cards/match?q=where%20is%20storage", token, status);
    check("Cards a request would bring up", status == 200 && match.getProperty("cards", {})[0].getProperty("id", {}).toString() == "suite.rule.vfs");
    const auto queued = call(baseUrl, "/v1/messages", token, status, "{\"content\":\"Where does storage go?\"}");
    const auto requestId = queued.getProperty("requestId", {}).toString();
    juce::var reply;
    for (int i = 0; i < 100 && requestId.isNotEmpty(); ++i)
    {
        reply = call(baseUrl, "/v1/requests/" + requestId, token, status);
        if (reply.getProperty("status", {}).toString() == "completed" || reply.getProperty("status", {}).toString() == "failed")
            break;
        juce::Thread::sleep(50);
    }
    check("A message through the API comes back completed with the reply",
          reply.getProperty("status", {}).toString() == "completed" && reply.getProperty("response", {}).toString().startsWith("Answer"));
    api.stop();
    check("Stopping removes the announcement",
          services::SuiteVfsJsonStore::loadJson(creation::agent::AgentApi::discoveryEntry(creation::assets::SuiteAppDomain::texture), error).isVoid());

    // Leave nothing behind; the real AI settings were never touched.
    services::SuiteVfsJsonStore::removeScopeForTesting(error);
    services::SuiteVfsJsonStore::setScopeForTesting({});
    juce::MemoryBlock afterAiSettings;
    const bool hasRealAiSettings = client.readEntry("ai-settings.json", afterAiSettings);
    check("The real AI settings were not touched", hasRealAiSettings == hadRealAiSettings && afterAiSettings == realAiSettings);

    std::cout << (failures == 0 ? "VirtualEngineer: ok" : "VirtualEngineer: FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
