#include <creation/agent/VirtualEngineer.h>

#include <creation/litesemrag/Embeddings.h>
#include <creation/services/SuiteAiProviderRuntime.h>

#include <cmath>
#include <thread>

namespace creation::agent
{
namespace ls = creation::litesemrag;
namespace services = creation::services;

namespace
{
constexpr int conversationToKeep = 6; // exchanges

juce::String appName(creation::assets::SuiteAppDomain app)
{
    auto token = creation::assets::toStorageToken(app);
    return token.isEmpty() ? juce::String("the suite") : "Djehuti " + token.substring(0, 1).toUpperCase() + token.substring(1);
}

juce::var cardsUsed(const ls::Retrieval& retrieval)
{
    juce::Array<juce::var> cards;
    for (const auto& retrieved : retrieval.cards)
    {
        auto* card = new juce::DynamicObject();
        card->setProperty("id", retrieved.card.id);
        card->setProperty("scope", ls::scopeName(retrieved.scope));
        card->setProperty("kind", retrieved.card.kind);
        card->setProperty("title", retrieved.card.title);
        card->setProperty("priority", retrieved.card.priority);
        if (retrieved.meaning >= 0.0f)
            card->setProperty("meaning", std::round(retrieved.meaning * 1000.0f) / 1000.0f);
        juce::Array<juce::var> words;
        for (const auto& word : retrieved.wordsMatched)
            words.add(word);
        card->setProperty("words", juce::var(words));
        cards.add(juce::var(card));
    }
    return juce::var(cards);
}

// The suite's AI account for this app - entered once in the suite's settings, never asked for by an app. Empty
// accountId, with `problem` saying why, when there is none to use.
struct Account
{
    services::SuiteAiResolvedRuntimeSettings runtime;
    juce::String problem;
};

Account accountFor(creation::assets::SuiteAppDomain app)
{
    Account account;
    juce::String settingsError;
    const auto settings = services::SuiteAiSettingsStore().load(settingsError);
    account.runtime = services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(settings, app);
    const auto profile = services::SuiteAiProviderRuntime::resolveProfile(account.runtime.providerId);
    if (settingsError.isNotEmpty())
        account.problem = "The suite's AI settings could not be read: " + settingsError;
    else if (account.runtime.accountId.isEmpty() || services::SuiteAiProviderRuntime::requiresApiKey(profile, account.runtime.apiKey))
        account.problem = "No AI account is set up for this app. Add one in the suite's settings (AI Accounts); every app uses it.";
    if (account.problem.isNotEmpty())
        account.runtime.accountId = {};
    return account;
}

using Embedder = std::function<bool(const services::SuiteAiResolvedRuntimeSettings&, const juce::StringArray&,
                                    services::SuiteAiEmbeddingClient::Result&)>;

// The cards for a request, matched by meaning when the account's provider can embed, else on words alone (the
// retrieval says why). The request is embedded every time; a card only the first time its words are seen, after which
// its embedding comes from the cache in the VFS.
ls::Retrieval retrieveCards(const juce::String& prompt, const std::vector<ls::ScopedCards>& scopes, const Account& account,
                            const Embedder& embedderForTesting, juce::String& warning)
{
    auto wordsOnly = [&](const juce::String& because) {
        auto retrieval = ls::retrieve(prompt, scopes);
        retrieval.wordsOnlyBecause = because;
        return retrieval;
    };
    if (prompt.trim().isEmpty())
        return ls::retrieve(prompt, scopes);
    if (account.runtime.accountId.isEmpty())
        return wordsOnly(account.problem);

    // A test's embedder stands in for a measured model, at OpenAI's values.
    const auto embedding = embedderForTesting ? services::SuiteAiEmbeddingClient::EmbeddingModel { "test-embedder", 0.18f, 0.08f }
                                              : services::SuiteAiEmbeddingClient::embeddingModelFor(account.runtime);
    const auto& model = embedding.name;
    if (model.isEmpty())
        return wordsOnly(services::SuiteAiProviderRuntime::resolveProfile(account.runtime.providerId).displayName
                         + " has no embedding model the suite has measured, so cards are matched on words alone.");

    juce::String cacheError;
    auto cache = ls::EmbeddingCache::load(model, cacheError);
    if (cacheError.isNotEmpty())
        warning = "The card embeddings could not be read: " + cacheError;

    juce::StringArray texts { prompt };
    juce::StringArray newKeys;
    for (const auto& scoped : scopes)
        for (const auto& card : scoped.cards)
        {
            if (! card.isActive() || card.text.trim().isEmpty())
                continue;
            const auto key = ls::embeddingKey(card, model);
            if (cache.find(key) == nullptr && ! newKeys.contains(key))
            {
                newKeys.add(key);
                texts.add(ls::embeddingText(card));
            }
        }

    services::SuiteAiEmbeddingClient::Result embedded;
    const bool ok = embedderForTesting ? embedderForTesting(account.runtime, texts, embedded)
                                       : services::SuiteAiEmbeddingClient().embed(account.runtime, texts, embedded);
    if (! ok || embedded.vectors.size() != static_cast<size_t>(texts.size()))
        return wordsOnly("Matching by meaning failed, so cards were matched on words alone: "
                         + (embedded.errorMessage.isNotEmpty() ? embedded.errorMessage : juce::String("no embeddings came back.")));

    for (int i = 0; i < newKeys.size(); ++i)
        cache.put(newKeys[i], embedded.vectors[static_cast<size_t>(i + 1)]);
    juce::String saveError;
    if (! cache.save(saveError))
        warning = "The card embeddings could not be saved: " + saveError;

    ls::MeaningMatch meaning;
    meaning.query = embedded.vectors.front();
    meaning.floor = embedding.floor;
    meaning.margin = embedding.margin;
    meaning.cardVector = [&cache, &model](const ls::Card& card) { return cache.find(ls::embeddingKey(card, model)); };
    return ls::retrieve(prompt, scopes, 6, &meaning);
}

// The scopes as they stand. A free function: the request's worker uses it without the engineer, which may be gone
// before the worker finishes.
std::vector<ls::ScopedCards> cardScopes(creation::assets::SuiteAppDomain app, const juce::Array<ls::Card>& shipped,
                                        const juce::String& projectId, juce::String& errorMessage)
{
    std::vector<ls::ScopedCards> scopes;
    scopes.push_back({ ls::CardScope::shipped, shipped });
    scopes.push_back({ ls::CardScope::suite, ls::CardStore::load(ls::CardStore::suite(), errorMessage) });
    scopes.push_back({ ls::CardScope::app, ls::CardStore::load(ls::CardStore::forApp(app), errorMessage) });
    if (projectId.isNotEmpty())
        scopes.push_back({ ls::CardScope::project, ls::CardStore::load(ls::CardStore::forProject(projectId), errorMessage) });
    return scopes;
}

juce::String systemPrompt(creation::assets::SuiteAppDomain app, const ls::Retrieval& retrieval)
{
    juce::String system;
    system << "You are the Virtual Engineer of the Djehuti Creation Suite, working in " << appName(app) << ". "
           << "Answer from the context you are given and say plainly when you do not know. "
           << "Report what you did, what you checked and what failed; never claim something you have not verified.\n";
    if (retrieval.context.isNotEmpty())
        system << "\n" << retrieval.context;
    return system;
}
} // namespace

VirtualEngineer::VirtualEngineer(creation::assets::SuiteAppDomain appDomain) : app(appDomain) {}

VirtualEngineer::~VirtualEngineer()
{
    ++*generation; // a reply still on its way is dropped
}

void VirtualEngineer::setProjectId(const juce::String& id)
{
    std::lock_guard guard(lock);
    projectId = id;
}

juce::String VirtualEngineer::getProjectId() const
{
    std::lock_guard guard(lock);
    return projectId;
}

void VirtualEngineer::setShippedCards(juce::Array<ls::Card> cards)
{
    std::lock_guard guard(lock);
    shippedCards = std::move(cards);
}

void VirtualEngineer::cancel()
{
    ++*generation;
    busy = false;
}

void VirtualEngineer::clearConversation()
{
    std::lock_guard guard(lock);
    conversation.clear();
}

std::vector<ls::ScopedCards> VirtualEngineer::loadCards(juce::String& errorMessage) const
{
    juce::Array<ls::Card> shipped;
    juce::String project;
    {
        std::lock_guard guard(lock);
        shipped = shippedCards;
        project = projectId;
    }
    return cardScopes(app, shipped, project, errorMessage);
}

ls::Retrieval VirtualEngineer::retrieveFor(const juce::String& prompt, juce::String& errorMessage) const
{
    const auto scopes = loadCards(errorMessage);
    juce::String warning;
    auto retrieval = retrieveCards(prompt, scopes, accountFor(app), embedderForTesting, warning);
    if (errorMessage.isEmpty())
        errorMessage = warning;
    return retrieval;
}

void VirtualEngineer::retrieveAsync(const juce::String& prompt,
                                    std::function<void(const ls::Retrieval& retrieval, const juce::String& error)> done) const
{
    juce::String loadError;
    auto scopes = loadCards(loadError);
    const auto appDomain = app;
    const auto embedder = embedderForTesting;
    const bool direct = deliverDirectlyForTesting;
    std::thread([prompt, scopes = std::move(scopes), loadError, appDomain, embedder, direct, done = std::move(done)]() {
        juce::String warning;
        auto retrieval = retrieveCards(prompt, scopes, accountFor(appDomain), embedder, warning);
        const auto error = loadError.isNotEmpty() ? loadError : warning;
        auto finish = [retrieval = std::move(retrieval), error, done]() {
            if (done)
                done(retrieval, error);
        };
        if (direct)
            finish();
        else
            juce::MessageManager::callAsync(std::move(finish));
    }).detach();
}

bool VirtualEngineer::ask(const juce::String& prompt, Completion completion)
{
    if (prompt.trim().isEmpty() || busy.exchange(true))
        return false;

    // What the app says about itself is read here, on the calling (message) thread, where its state lives.
    const auto context = appContext ? appContext(prompt) : juce::String();

    // Everything the worker needs, copied: it never touches the engineer.
    juce::Array<Exchange> earlier;
    juce::Array<ls::Card> shipped;
    juce::String project;
    {
        std::lock_guard guard(lock);
        earlier = conversation;
        shipped = shippedCards;
        project = projectId;
    }
    const auto appDomain = app;
    const auto transport = transportForTesting;
    const auto embedder = embedderForTesting;

    const int myGeneration = ++*generation;
    auto alive = generation;
    auto deliver = [this, alive, myGeneration, prompt, completion = std::move(completion)](AskResult result) {
        auto finish = [this, alive, myGeneration, prompt, completion, result]() {
            if (alive->load() != myGeneration)
                return; // cancelled, or the engineer is gone
            if (result.ok)
            {
                std::lock_guard guard(lock);
                conversation.add({ prompt, result.text });
                while (conversation.size() > conversationToKeep)
                    conversation.remove(0);
            }
            busy = false;
            if (completion)
                completion(result);
        };
        if (deliverDirectlyForTesting)
            finish();
        else
            juce::MessageManager::callAsync(std::move(finish));
    };

    std::thread([appDomain, prompt, context, earlier, shipped, project, transport, embedder, deliver]() {
        const auto started = juce::Time::getMillisecondCounterHiRes();
        AskResult result;
        auto* details = new juce::DynamicObject();
        result.details = juce::var(details);
        details->setProperty("request", prompt);
        details->setProperty("app", creation::assets::toStorageToken(appDomain));

        const auto account = accountFor(appDomain);
        if (account.runtime.accountId.isEmpty())
        {
            result.error = account.problem;
            deliver(result);
            return;
        }
        const auto& runtime = account.runtime;
        details->setProperty("account", runtime.accountId);
        details->setProperty("provider", runtime.providerId);
        details->setProperty("model", runtime.modelName);

        juce::String cardsError, cardsWarning;
        const auto retrieval = retrieveCards(prompt, cardScopes(appDomain, shipped, project, cardsError), account, embedder, cardsWarning);
        details->setProperty("cards", cardsUsed(retrieval));
        details->setProperty("matchedBy", retrieval.byMeaning ? "meaning and words" : "words");
        if (retrieval.wordsOnlyBecause.isNotEmpty())
            details->setProperty("wordsOnlyBecause", retrieval.wordsOnlyBecause);
        juce::Array<juce::var> tokens;
        for (const auto& token : retrieval.tokens)
            tokens.add(token);
        details->setProperty("tokens", juce::var(tokens));
        if (cardsError.isNotEmpty() || cardsWarning.isNotEmpty())
            details->setProperty("cardsWarning", cardsError.isNotEmpty() ? cardsError : cardsWarning);

        juce::String user;
        if (context.trim().isNotEmpty())
            user << "What is open in " << appName(appDomain) << " now:\n" << context.trim() << "\n\n";
        if (! earlier.isEmpty())
        {
            user << "Earlier in this conversation:\n";
            for (const auto& exchange : earlier)
                user << "User: " << exchange.request << "\nYou: " << exchange.reply << "\n";
            user << "\n";
        }
        user << "Request:\n" << prompt;

        services::SuiteAiChatClient::ChatResult chat;
        const auto system = systemPrompt(appDomain, retrieval);
        const bool ok = transport ? transport(runtime, system, user, chat)
                                  : services::SuiteAiChatClient().sendChatCompletion(runtime, system, user, chat);
        details->setProperty("durationMs", juce::Time::getMillisecondCounterHiRes() - started);
        result.ok = ok && chat.text.isNotEmpty();
        result.text = chat.text;
        if (! result.ok)
            result.error = chat.errorMessage.isNotEmpty() ? chat.errorMessage : juce::String("The AI provider returned no reply.");
        deliver(result);
    }).detach();
    return true;
}
} // namespace creation::agent
