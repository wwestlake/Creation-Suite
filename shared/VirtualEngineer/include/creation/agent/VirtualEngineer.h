#pragma once

#include <creation/agent/Tools.h>
#include <creation/litesemrag/CardStore.h>
#include <creation/litesemrag/Retrieval.h>
#include <creation/services/SuiteAiChatClient.h>
#include <creation/services/SuiteAiEmbeddingClient.h>
#include <creation/services/SuiteAiSettings.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

// The suite's Virtual Engineer (shared/VirtualEngineer/README.md): the one way any app asks the AI. The AI belongs to
// the suite - its accounts and keys are the suite's (ai-settings.json in the VFS), its guidance is LiteSemRAG cards -
// and an app supplies only its panel, its own context, its own API endpoints and the tools it lets the engineer use.
namespace creation::agent
{
struct AskResult
{
    bool ok = false;
    juce::String text;  // the reply, when ok
    juce::String error; // in words a person can act on, when not
    // model, account, the cards used and why, timing; with tools also the run: status (completed, partial, failed,
    // cancelled), every action (tool, arguments, result) and whether it can be undone
    juce::var details;
};

// What the user is asked before a destructive or external tool runs.
struct ApprovalRequest
{
    juce::String request;     // what the user asked for
    juce::String toolName;
    juce::String toolTitle;
    Effect effect = Effect::destructive;
    juce::String description; // what the tool does
    juce::String arguments;   // the call, as JSON text
};

class VirtualEngineer final
{
public:
    explicit VirtualEngineer(creation::assets::SuiteAppDomain app);
    ~VirtualEngineer();
    VirtualEngineer(const VirtualEngineer&) = delete;
    VirtualEngineer& operator=(const VirtualEngineer&) = delete;

    creation::assets::SuiteAppDomain getApp() const noexcept { return app; }

    // The open project, for its cards and its run records; empty when none is open.
    void setProjectId(const juce::String& projectId);
    juce::String getProjectId() const;

    // Cards built into the app from source (read-only).
    void setShippedCards(juce::Array<creation::litesemrag::Card> cards);

    // The app's own context for a request: what is open, selected, failing. Called on the message thread just before a
    // request goes out; plain text, summarised (the agent reads detail with the app's read tools).
    std::function<juce::String(const juce::String& prompt)> appContext;

    // ---- Acting (docs/architecture/Suite-Agent-Runtime-Spec.md, sections 4, 5, 8, 10). All on the message thread.
    // The tools the engineer may use. With none, or a provider that cannot be sent tools, it only talks.
    bool addTool(ToolDefinition definition, ToolHandler handler, juce::String& error);
    const ToolRegistry& getTools() const noexcept { return tools; }
    // State the tools change, captured before a request's first change so the whole request undoes at once. A write
    // tool is refused while no state domain is registered: nothing could undo it.
    void addStateDomain(StateDomain domain);
    // Asks the user before a destructive or external tool runs; call `decide` with the answer (now or later, on the
    // message thread). Without one, such tools are refused.
    std::function<void(const ApprovalRequest& request, std::function<void(bool allowed)> decide)> approver;

    // Undo of the last request that changed something: restores the state from before its first change.
    bool canUndoLastRequest() const noexcept { return lastChange != nullptr; }
    juce::String lastRequestLabel() const;
    enum class UndoOutcome
    {
        nothing,     // no request has changed anything (or a request is still running)
        undone,
        editedSince, // the state changed after the request ended; nothing restored unless evenIfEditedSince
    };
    UndoOutcome undoLastRequest(bool evenIfEditedSince);

    // Limits on one request.
    struct Limits
    {
        int modelCalls = 12;
        int toolCalls = 40;
        int sameCallRepeats = 3; // the same call with the same result, in a row: a loop
        int toolTimeoutMs = 60000;
        int approvalTimeoutMs = 10 * 60 * 1000;
    };
    Limits limits;

    using Completion = std::function<void(const AskResult& result)>;

    // Sends a request: the suite account and key for this app, the cards that match, the app's context and the recent
    // conversation. With tools, the model may act - each call checked, gated by its effect, run on the message thread,
    // its result sent back - until it answers or a limit stops it. The reply is delivered on the message thread. False
    // (and nothing sent) while another request is still running.
    bool ask(const juce::String& prompt, Completion completion);
    bool isBusy() const noexcept { return busy.load(); }
    // Stops the running request: no further tool runs; its report says what was done before it stopped. A model call
    // in flight is left to finish in the background and its answer dropped.
    void cancel();
    void clearConversation();

    // The cards a request would bring up, without asking the AI anything. Matching by meaning embeds the request
    // through the suite account (and any card not embedded yet), so this waits on the network: call it off the message
    // thread, or use retrieveAsync.
    creation::litesemrag::Retrieval retrieveFor(const juce::String& prompt, juce::String& errorMessage) const;
    // The same on a worker thread, delivered on the message thread (for the Cards panel).
    void retrieveAsync(const juce::String& prompt,
                       std::function<void(const creation::litesemrag::Retrieval& retrieval, const juce::String& error)> done) const;

    // The scopes as they stand: shipped, suite, this app, the open project.
    std::vector<creation::litesemrag::ScopedCards> loadCards(juce::String& errorMessage) const;

    // For tests: stand-ins for the provider (a plain completion, and a tool-calling turn), for its embeddings, and
    // running everything where it happens instead of on the message thread (a console test has no message loop).
    std::function<bool(const creation::services::SuiteAiResolvedRuntimeSettings& settings, const juce::String& systemPrompt,
                       const juce::String& userPrompt, creation::services::SuiteAiChatClient::ChatResult& result)>
        transportForTesting;
    std::function<bool(const creation::services::SuiteAiResolvedRuntimeSettings& settings,
                       const std::vector<creation::services::SuiteAiChatClient::Message>& messages,
                       const std::vector<creation::services::SuiteAiChatClient::ToolSpec>& tools,
                       creation::services::SuiteAiChatClient::TurnResult& result)>
        turnTransportForTesting;
    bool deliverDirectlyForTesting = false;
    std::function<bool(const creation::services::SuiteAiResolvedRuntimeSettings& settings, const juce::StringArray& texts,
                       creation::services::SuiteAiEmbeddingClient::Result& result)>
        embedderForTesting;

    // The request's worker reaches the engineer only through this (it may outlive the engineer). Internal.
    struct Bridge;

private:
    struct Exchange
    {
        juce::String request;
        juce::String reply;
    };
    struct Change; // one request's undo: the state before its first change, and after it ended
    struct Run;    // the running request's control: stop

    std::vector<juce::var> captureState() const;
    void restoreState(const std::vector<juce::var>& state);

    const creation::assets::SuiteAppDomain app;
    mutable std::mutex lock;
    juce::String projectId;
    juce::Array<creation::litesemrag::Card> shippedCards;
    juce::Array<Exchange> conversation; // the last few exchanges
    std::atomic<bool> busy { false };

    ToolRegistry tools;
    std::vector<StateDomain> stateDomains;
    std::shared_ptr<Change> lastChange;   // message thread
    std::shared_ptr<Run> currentRun;
    std::shared_ptr<Bridge> bridge;
};
} // namespace creation::agent
