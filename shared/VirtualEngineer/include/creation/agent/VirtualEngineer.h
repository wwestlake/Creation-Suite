#pragma once

#include <creation/litesemrag/CardStore.h>
#include <creation/litesemrag/Retrieval.h>
#include <creation/services/SuiteAiChatClient.h>
#include <creation/services/SuiteAiSettings.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// The suite's Virtual Engineer (shared/VirtualEngineer/README.md): the one way any app asks the AI. The AI belongs to
// the suite - its accounts and keys are the suite's (ai-settings.json in the VFS), its guidance is LiteSemRAG cards -
// and an app supplies only its panel, its own context and its own API endpoints.
namespace creation::agent
{
struct AskResult
{
    bool ok = false;
    juce::String text;  // the reply, when ok
    juce::String error; // in words a person can act on, when not
    juce::var details;  // model, account, the cards used and why, timing
};

class VirtualEngineer final
{
public:
    explicit VirtualEngineer(creation::assets::SuiteAppDomain app);
    ~VirtualEngineer();
    VirtualEngineer(const VirtualEngineer&) = delete;
    VirtualEngineer& operator=(const VirtualEngineer&) = delete;

    creation::assets::SuiteAppDomain getApp() const noexcept { return app; }

    // The open project, for its cards; empty when none is open.
    void setProjectId(const juce::String& projectId);
    juce::String getProjectId() const;

    // Cards built into the app from source (read-only).
    void setShippedCards(juce::Array<creation::litesemrag::Card> cards);

    // The app's own context for a request: what is open, selected, failing. Called on the message thread just before a
    // request goes out; plain text, summarised (the agent asks for detail through the app's API endpoints).
    std::function<juce::String(const juce::String& prompt)> appContext;

    using Completion = std::function<void(const AskResult& result)>;

    // Sends a request: the suite account and key for this app, the cards that match, the app's context and the recent
    // conversation, then the reply - delivered on the message thread. False (and nothing sent) while another request is
    // still running.
    bool ask(const juce::String& prompt, Completion completion);
    bool isBusy() const noexcept { return busy.load(); }
    // Stops waiting for the running request: its reply is dropped and the engineer is free again.
    void cancel();
    void clearConversation();

    // The cards a request would bring up, without sending anything (for the API and the Cards panel).
    creation::litesemrag::Retrieval retrieveFor(const juce::String& prompt, juce::String& errorMessage) const;

    // The scopes as they stand: shipped, suite, this app, the open project.
    std::vector<creation::litesemrag::ScopedCards> loadCards(juce::String& errorMessage) const;

    // For tests: a stand-in for the provider, and running completions where they happen instead of on the message
    // thread (a console test has no message loop).
    std::function<bool(const creation::services::SuiteAiResolvedRuntimeSettings& settings, const juce::String& systemPrompt,
                       const juce::String& userPrompt, creation::services::SuiteAiChatClient::ChatResult& result)>
        transportForTesting;
    bool deliverDirectlyForTesting = false;

private:
    struct Exchange
    {
        juce::String request;
        juce::String reply;
    };

    juce::String systemPromptFor(const creation::litesemrag::Retrieval& retrieval) const;

    const creation::assets::SuiteAppDomain app;
    mutable std::mutex lock;
    juce::String projectId;
    juce::Array<creation::litesemrag::Card> shippedCards;
    juce::Array<Exchange> conversation; // the last few exchanges
    std::atomic<bool> busy { false };
    std::shared_ptr<std::atomic<int>> generation = std::make_shared<std::atomic<int>>(0); // cancel / destruction
};
} // namespace creation::agent
