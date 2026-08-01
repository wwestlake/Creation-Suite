#pragma once

#include <juce_core/juce_core.h>

namespace creation::services
{
// One retrievable piece of suite/project state. Contributed by any app
// via a SuiteContextProvider -- the engine itself knows nothing about
// what a Patina buffer or a CEL script is, only that it's a document
// with a category and a body.
struct SuiteContextDocument
{
    juce::String id;
    juce::String title;
    juce::String category;
    juce::String body;
    juce::String sourceApp;
    juce::String sourcePath;
    juce::StringArray tags;
    juce::Time updatedAt;
};

// Any app implements this to feed its own documents into the shared
// engine without the engine needing app-specific knowledge. Non-owning:
// the engine holds a raw pointer for the provider's registered lifetime;
// the owning app is responsible for unregistering before destruction.
class SuiteContextProvider
{
public:
    virtual ~SuiteContextProvider() = default;
    virtual juce::Array<SuiteContextDocument> CollectDocuments() = 0;
};

struct SuiteContextRetrievalRequest
{
    juce::String prompt;
    juce::String systemPrompt; // for structural-separation (alpha); empty is valid, alpha reports 0.
    juce::String processScope; // CTX-2 hook: which task/process this belongs to.
    juce::String appDomain;
    int maxItems = 6;
};

// Adapted from Westlake, "Information Space Dynamics in Large Language
// Models: A Unified Framework" (v5). This measures PROMPT trajectory
// dynamics (successive user prompts within a session), not RESPONSE
// trajectory dynamics -- the paper's original formulation is about
// diagnosing a model's own response behavior (safety/hallucination
// monitoring); this engine runs before generation, to decide how to
// assemble retrieval context, so the analogous signal is "is the user's
// own line of questioning staying on-topic or pivoting hard," which is
// exactly what should widen or narrow retrieval. Explicit adaptation,
// not a claim of implementing the paper's original diagnostic use case.
//
// beta (instruction leakage) needs token-level perplexity/logprob
// access most BYOK providers don't expose -- reported as unavailable
// rather than a fabricated number when that data isn't supplied.
struct SuiteContextDynamics
{
    float structuralSeparation = 0.0f; // alpha: Jaccard distance, systemPrompt tokens vs prompt tokens.
    float trajectorySkew = 0.0f;       // gamma: variance of lexical density over the recent prompt window.
    float referenceDrift = 0.0f;       // delta: 1 - Jaccard similarity of current prompt vs the session anchor prompt.
    float velocity = 0.0f;             // v: 1 - Jaccard similarity of current prompt vs the previous prompt.
    float curvature = 0.0f;            // kappa: turning angle (radians) between successive token-frequency displacement vectors.
    int torsionalResistance = 0;       // tau: consecutive-turn count spent in a suggested-recovery state.
    bool recoverySuggested = false;

    bool instructionLeakageAvailable = false;
    float instructionLeakage = 0.0f; // beta, only meaningful when instructionLeakageAvailable is true.
};

struct SuiteContextSnippet
{
    juce::String documentId;
    juce::String title;
    juce::String category;
    juce::String sourceApp;
    juce::String excerpt;
    float relevanceScore = 0.0f;
};

struct SuiteContextPacket
{
    SuiteContextRetrievalRequest request;
    SuiteContextDynamics dynamics;
    juce::String summary;
    juce::Array<SuiteContextSnippet> snippets;
};

// Suite-wide context assembly: any app registers a SuiteContextProvider,
// any app submits retrieval requests, one shared engine ranks documents
// and computes trajectory dynamics. All retrieval work (tokenizing,
// ranking, dynamics) runs on this class's own background thread -- never
// the caller's UI/real-time thread. Token-overlap ranking is the
// deliberate Phase-1 baseline (real embeddings are a later phase, same
// as the app-local scaffold this replaces).
class SuiteContextEngine final : private juce::Thread
{
public:
    SuiteContextEngine();
    ~SuiteContextEngine() override;

    // Non-owning; caller must UnregisterProvider before destroying the
    // provider (same convention as JUCE's own Listener registration).
    void RegisterProvider(SuiteContextProvider* provider);
    void UnregisterProvider(SuiteContextProvider* provider);

    void SubmitRequest(const SuiteContextRetrievalRequest& request);
    SuiteContextPacket GetLastPacket() const;

    // Fires on THIS class's own background thread, not the caller's
    // message/UI thread -- shared/Services deliberately links only
    // juce_core (usable from headless contexts), not juce_events, so
    // there is no juce::MessageManager to hop through here. A GUI
    // caller that needs this on its message thread is responsible for
    // its own marshaling (e.g. juce::MessageManager::callAsync from
    // inside its own callback), same as ViewportComponent's
    // RunOnGLThread pattern puts the hop on the caller, not the callee.
    std::function<void(const SuiteContextPacket&)> onContextReady;

    static constexpr int kTrajectoryWindow = 8; // recent-prompt window for trajectorySkew (gamma).
    static constexpr float kRecoveryDriftThreshold = 0.72f;
    static constexpr float kRecoveryCurvatureThreshold = 0.45f;

private:
    void run() override;

    SuiteContextPacket BuildPacket(const SuiteContextRetrievalRequest& request);
    SuiteContextDynamics ComputeDynamics(const SuiteContextRetrievalRequest& request);
    juce::String BuildSummary(const SuiteContextPacket& packet) const;

    mutable juce::CriticalSection lock_;
    juce::WaitableEvent wakeEvent_;
    juce::Array<SuiteContextProvider*> providers_;
    SuiteContextRetrievalRequest pendingRequest_;
    bool hasPendingRequest_ = false;
    SuiteContextPacket lastPacket_;

    // Dynamics state, carried across successive SubmitRequest calls
    // within this engine instance's lifetime -- one engine instance is
    // one continuous session/trajectory.
    juce::StringArray anchorTokens_;
    juce::StringArray previousTokens_;
    juce::Array<juce::StringArray> recentPromptWindow_;
    std::map<juce::String, int> previousFrequency_;
    std::map<juce::String, int> previousDisplacement_;
    bool hasPreviousDisplacement_ = false;
    int torsionalResistance_ = 0;
};
}
