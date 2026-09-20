#pragma once

#include "Provider.h"
#include "ToolRegistry.h"

#include <functional>
#include <string>
#include <vector>

namespace creation::ai
{
// Budgets and safeguards for one run. A run that reaches one ends with a specific reason and everything it
// did is kept, so the user can see how far it got.
struct AgentLimits
{
    int maxSteps = 25;                       // model calls in one run
    int maxToolCallsPerStep = 8;             // more than this in one reply are refused
    double maxSeconds = 600.0;               // wall-clock time for the whole run
    std::int64_t maxTotalTokens = 0;         // input + output over the run; 0 = no limit
    int maxConsecutiveFailedSteps = 4;       // steps in a row where every tool call failed
    int maxRepeatedIdenticalCalls = 2;       // the same tool with the same arguments this many times is allowed; the next is refused
    int maxProviderRetries = 3;              // retries of a retryable provider error
    double retryBaseDelaySeconds = 1.0;      // doubled each retry, unless the provider said how long to wait
    double retryMaxDelaySeconds = 30.0;
    int maxContinuations = 3;                // "continue" nudges when a reply is cut off by the output limit
    double contextBudgetFraction = 0.8;      // of the model's context window, when it is known
};

enum class EndReason
{
    finished,            // the model answered without asking for more tools
    stopped,             // the user stopped it
    stepLimit,
    timeLimit,
    tokenLimit,
    providerError,       // see detail
    loopDetected,        // it kept repeating the same failing call
    tooManyFailures,     // step after step of tool failures
    unsupported          // the provider cannot do what the run needs
};

// A decision about running one tool call, made by the host (which may ask the user and wait).
struct ToolDecision
{
    bool allow = true;
    std::string reason;    // shown to the model when denied
};

using ToolPolicy = std::function<ToolDecision(const Tool& tool, const ToolCallBlock& call)>;

// What a host watches to show the run as it happens. All optional. Called on the thread running the engine.
struct AgentEvents
{
    std::function<void(int step)> onStepStarted;
    std::function<void(const std::string& textDelta)> onText;                  // streamed reply text
    std::function<void(const ToolCallBlock& call)> onToolCall;                 // about to run
    std::function<void(const ToolCallBlock& call, const ToolResult& result)> onToolResult;
    std::function<void(const std::string& message)> onNotice;                  // retries, trimming, refusals
};

struct AgentRequest
{
    std::string model;
    std::string system;
    std::vector<Message> history;        // the conversation before this request
    std::string userMessage;             // what the user just asked
    AgentLimits limits;
    ToolPolicy policy;                   // null: every tool is allowed
    AgentEvents events;
    double temperature = 0.2;
    int maxOutputTokens = 4096;
};

struct AgentResult
{
    EndReason reason = EndReason::finished;
    std::string detail;                  // why, in words, when it did not simply finish
    std::string finalText;               // the model's last answer
    std::vector<Message> messages;       // everything added this run: the user message, replies, tool results
    Usage usage;
    int steps = 0;
    int toolCalls = 0;
};

// The agent loop: ask the model, run the tools it calls, give the results back, repeat until it answers or
// a limit is reached. It knows nothing about any provider's protocol or any application: only Provider and
// ToolRegistry. run() blocks; call it off the application's main thread. Stop it through the CancelToken.
class AgentEngine
{
public:
    AgentEngine(Provider& provider, ToolRegistry& tools);

    AgentResult run(const AgentRequest& request, const CancelToken& cancel);

    // Drops the oldest tool exchanges and replies until the conversation fits `maxChars` (a rough size: about
    // four characters per token), always keeping the first message and the most recent ones, and never
    // separating a tool call from its result. Returns how many messages were removed.
    static int trimToFit(std::vector<Message>& messages, size_t maxChars);
    static size_t sizeOf(const std::vector<Message>& messages);

private:
    Provider& provider;
    ToolRegistry& tools;
};
}
