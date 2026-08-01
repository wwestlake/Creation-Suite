#include <creation/services/SuiteContextEngine.h>

#include <cmath>
#include <map>

namespace
{
using creation::services::SuiteContextDocument;

juce::StringArray Tokenize(const juce::String& text)
{
    auto normalized = text.toLowerCase();
    normalized = normalized.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789 _-@./");

    juce::StringArray tokens;
    tokens.addTokens(normalized, " \r\n\t", "");
    tokens.trim();
    tokens.removeEmptyStrings();
    return tokens;
}

float JaccardSimilarity(const juce::StringArray& a, const juce::StringArray& b)
{
    juce::StringArray uniqueA(a);
    juce::StringArray uniqueB(b);
    uniqueA.removeDuplicates(false);
    uniqueB.removeDuplicates(false);

    if (uniqueA.isEmpty() && uniqueB.isEmpty())
        return 1.0f;

    int intersection = 0;
    for (const auto& token : uniqueA)
        if (uniqueB.contains(token))
            ++intersection;

    auto unionCount = uniqueA.size();
    for (const auto& token : uniqueB)
        if (! uniqueA.contains(token))
            ++unionCount;

    return unionCount > 0 ? (float) intersection / (float) unionCount : 0.0f;
}

float LexicalDensity(const juce::StringArray& tokens)
{
    if (tokens.isEmpty())
        return 0.0f;

    juce::StringArray unique(tokens);
    unique.removeDuplicates(false);
    return (float) unique.size() / (float) tokens.size();
}

std::map<juce::String, int> TokenFrequency(const juce::StringArray& tokens)
{
    std::map<juce::String, int> frequency;
    for (const auto& token : tokens)
        ++frequency[token];
    return frequency;
}

// curr - prev over the union of keys -- a real displacement vector in
// the shared (sparse) vocabulary space, standing in for the paper's
// u(t) = R(t) - R(t-1) in the absence of a real embedding space in this
// Phase-1 implementation.
std::map<juce::String, int> FrequencyDelta(const std::map<juce::String, int>& curr,
                                           const std::map<juce::String, int>& prev)
{
    std::map<juce::String, int> delta = curr;
    for (const auto& [token, count] : prev)
        delta[token] -= count;
    return delta;
}

float CosineSimilarity(const std::map<juce::String, int>& a, const std::map<juce::String, int>& b)
{
    double dot = 0.0, magA = 0.0, magB = 0.0;
    for (const auto& [token, count] : a)
    {
        magA += (double) count * count;
        auto it = b.find(token);
        if (it != b.end())
            dot += (double) count * it->second;
    }
    for (const auto& [token, count] : b)
        magB += (double) count * count;

    if (magA <= 0.0 || magB <= 0.0)
        return 1.0f; // both-zero or one-zero displacement: no turn to measure, treat as no rotation.

    return (float) (dot / (std::sqrt(magA) * std::sqrt(magB)));
}

double PopulationVariance(const std::vector<double>& values)
{
    if (values.size() < 2)
        return 0.0;

    double mean = 0.0;
    for (double value : values)
        mean += value;
    mean /= (double) values.size();

    double variance = 0.0;
    for (double value : values)
        variance += (value - mean) * (value - mean);
    return variance / (double) values.size();
}

bool DocumentMatchesAnyCategory(const SuiteContextDocument& document, const juce::StringArray& categories)
{
    if (categories.isEmpty())
        return false;
    if (categories.contains(document.category, true))
        return true;
    for (const auto& tag : document.tags)
        if (categories.contains(tag, true))
            return true;
    return false;
}

juce::String MakeExcerpt(const juce::String& sourceText, const juce::StringArray& tokens)
{
    auto trimmed = sourceText.trim();
    if (trimmed.length() <= 220)
        return trimmed;

    for (const auto& token : tokens)
    {
        auto index = trimmed.toLowerCase().indexOf(token.toLowerCase());
        if (index >= 0)
        {
            auto start = juce::jmax(0, index - 60);
            return trimmed.substring(start, juce::jmin(trimmed.length(), start + 220)).trim();
        }
    }

    return trimmed.substring(0, 220).trim();
}
}

namespace creation::services
{
SuiteContextEngine::SuiteContextEngine() : juce::Thread("SuiteContextEngine")
{
    startThread();
}

SuiteContextEngine::~SuiteContextEngine()
{
    signalThreadShouldExit();
    wakeEvent_.signal();
    stopThread(4000);
}

void SuiteContextEngine::RegisterProvider(SuiteContextProvider* provider)
{
    const juce::ScopedLock sl(lock_);
    providers_.addIfNotAlreadyThere(provider);
}

void SuiteContextEngine::UnregisterProvider(SuiteContextProvider* provider)
{
    const juce::ScopedLock sl(lock_);
    providers_.removeAllInstancesOf(provider);
}

void SuiteContextEngine::SubmitRequest(const SuiteContextRetrievalRequest& request)
{
    {
        const juce::ScopedLock sl(lock_);
        pendingRequest_ = request;
        hasPendingRequest_ = true;
    }
    wakeEvent_.signal();
}

SuiteContextPacket SuiteContextEngine::GetLastPacket() const
{
    const juce::ScopedLock sl(lock_);
    return lastPacket_;
}

void SuiteContextEngine::run()
{
    while (! threadShouldExit())
    {
        wakeEvent_.wait(-1);
        if (threadShouldExit())
            break;

        SuiteContextRetrievalRequest request;
        {
            const juce::ScopedLock sl(lock_);
            if (! hasPendingRequest_)
                continue;
            request = pendingRequest_;
            hasPendingRequest_ = false;
        }

        auto packet = BuildPacket(request);

        {
            const juce::ScopedLock sl(lock_);
            lastPacket_ = packet;
        }

        if (onContextReady)
            onContextReady(packet);
    }
}

SuiteContextPacket SuiteContextEngine::BuildPacket(const SuiteContextRetrievalRequest& request)
{
    SuiteContextPacket packet;
    packet.request = request;
    packet.dynamics = ComputeDynamics(request);

    juce::Array<SuiteContextDocument> documents;
    {
        const juce::ScopedLock sl(lock_);
        for (auto* provider : providers_)
            documents.addArray(provider->CollectDocuments());
    }

    // CTX-2: a steering note folds into the query used for RANKING only
    // -- it never touches ComputeDynamics above, since dynamics track
    // the user's own conversational trajectory, not instruction text
    // attached alongside it.
    const auto& instruction = request.processInstruction;
    auto promptTokens = Tokenize(request.prompt);
    auto queryTokens = promptTokens;
    if (instruction.steeringNote.isNotEmpty())
        queryTokens.addArray(Tokenize(instruction.steeringNote));

    struct RankedItem
    {
        SuiteContextSnippet snippet;
        float score = 0.0f;
    };

    juce::Array<RankedItem> rankedItems;
    for (const auto& document : documents)
    {
        // Excluded categories/tags drop the document entirely, before
        // it can compete on score at all -- this is a hard filter, not
        // a down-rank.
        if (DocumentMatchesAnyCategory(document, instruction.excludeCategories))
            continue;

        auto docTokens = Tokenize(document.title + " " + document.category + " " + document.body + " "
                                   + document.tags.joinIntoString(" "));
        auto tokenScore = JaccardSimilarity(queryTokens, docTokens);
        auto freshnessHours = juce::jmax(0.0, document.updatedAt.toMilliseconds() > 0
                                                  ? (juce::Time::getCurrentTime() - document.updatedAt).inHours()
                                                  : 72.0);
        auto freshnessBoost = (float) juce::jmap(juce::jlimit(0.0, 72.0, freshnessHours), 72.0, 0.0, 0.0, 0.15);
        auto appBoost = document.sourceApp.equalsIgnoreCase(request.appDomain) ? 0.10f : 0.0f;
        auto instructionBoost =
            (DocumentMatchesAnyCategory(document, instruction.boostCategories) ? instruction.boostWeight : 0.0f)
            + (instruction.prioritizedSourceApps.contains(document.sourceApp, true) ? instruction.boostWeight : 0.0f);

        // Same guard as the app-local scaffold this generalizes: freshness/app boosts are
        // tie-breakers, not a relevance substitute -- without this, session-scoped documents
        // re-stamped to "now" every request would win regardless of what was actually asked.
        // An explicit user instruction is deliberately NOT gated behind this floor -- the user
        // asked for a category to be prioritized, so it should apply even to a weak text match.
        constexpr float minimumTokenScoreForBoost = 0.05f;
        auto score = tokenScore + instructionBoost;
        if (tokenScore >= minimumTokenScoreForBoost)
            score += freshnessBoost + appBoost;

        if (score <= 0.01f)
            continue;

        RankedItem item;
        item.score = score;
        item.snippet.documentId = document.id;
        item.snippet.title = document.title;
        item.snippet.category = document.category;
        item.snippet.sourceApp = document.sourceApp;
        item.snippet.relevanceScore = score;
        item.snippet.excerpt = MakeExcerpt(document.body, promptTokens);
        rankedItems.add(item);
    }

    std::sort(rankedItems.begin(), rankedItems.end(),
              [](const RankedItem& left, const RankedItem& right) { return left.score > right.score; });

    for (int index = 0; index < juce::jmin(request.maxItems, rankedItems.size()); ++index)
        packet.snippets.add(rankedItems.getReference(index).snippet);

    packet.summary = BuildSummary(packet);
    return packet;
}

SuiteContextDynamics SuiteContextEngine::ComputeDynamics(const SuiteContextRetrievalRequest& request)
{
    // Only ever called from run()'s thread (via BuildPacket) -- these
    // trajectory-state members are never touched from any other thread,
    // so no lock is needed here (unlike lock_-guarded members shared
    // with the calling thread).
    SuiteContextDynamics state;

    auto promptTokens = Tokenize(request.prompt);

    // alpha: structural separation. Not applicable (reported as 0, not
    // a fabricated distance) when there's no system prompt to erode
    // away from.
    if (request.systemPrompt.isNotEmpty())
    {
        auto systemTokens = Tokenize(request.systemPrompt);
        state.structuralSeparation = 1.0f - JaccardSimilarity(systemTokens, promptTokens);
    }

    if (anchorTokens_.isEmpty())
        anchorTokens_ = promptTokens;

    state.referenceDrift = 1.0f - JaccardSimilarity(anchorTokens_, promptTokens);
    state.velocity = 1.0f - JaccardSimilarity(previousTokens_, promptTokens);

    auto currentFrequency = TokenFrequency(promptTokens);
    auto displacement = FrequencyDelta(currentFrequency, previousFrequency_);
    if (hasPreviousDisplacement_)
    {
        auto cosine = juce::jlimit(-1.0f, 1.0f, CosineSimilarity(displacement, previousDisplacement_));
        state.curvature = std::acos(cosine);
    }
    previousDisplacement_ = displacement;
    hasPreviousDisplacement_ = true;
    previousFrequency_ = currentFrequency;

    recentPromptWindow_.add(promptTokens);
    while (recentPromptWindow_.size() > kTrajectoryWindow)
        recentPromptWindow_.remove(0);

    std::vector<double> densities;
    densities.reserve((size_t) recentPromptWindow_.size());
    for (const auto& windowTokens : recentPromptWindow_)
        densities.push_back((double) LexicalDensity(windowTokens));
    state.trajectorySkew = (float) PopulationVariance(densities);

    state.recoverySuggested =
        state.referenceDrift > kRecoveryDriftThreshold && state.curvature > kRecoveryCurvatureThreshold;

    // tau: depth-of-attractor proxy. This engine never itself issues a
    // "shock" (that's a downstream action -- e.g. a caller broadening
    // retrieval) so torsionalResistance_ tracks how many consecutive
    // turns recovery has been suggested without the trajectory actually
    // escaping (drift/curvature falling back below threshold); it
    // resets the moment the trajectory frees itself, at which point the
    // count just prior to reset was the escaped attractor's depth.
    torsionalResistance_ = state.recoverySuggested ? torsionalResistance_ + 1 : 0;
    state.torsionalResistance = torsionalResistance_;

    // beta (instruction leakage) needs token-level perplexity/logprob
    // access this engine has no way to obtain from a plain prompt
    // string -- explicitly left unavailable rather than guessed.
    state.instructionLeakageAvailable = false;

    previousTokens_ = promptTokens;
    return state;
}

juce::String SuiteContextEngine::BuildSummary(const SuiteContextPacket& packet) const
{
    juce::String summary;
    summary << "Context packet ready\n";
    summary << "Domain: " << packet.request.appDomain
            << "  |  Process: " << (packet.request.processScope.isNotEmpty() ? packet.request.processScope : "none")
            << "\n";
    summary << "Velocity: " << juce::String(packet.dynamics.velocity, 2)
            << "  |  Drift: " << juce::String(packet.dynamics.referenceDrift, 2)
            << "  |  Curvature: " << juce::String(packet.dynamics.curvature, 2)
            << "  |  Skew: " << juce::String(packet.dynamics.trajectorySkew, 3)
            << "  |  Tau: " << packet.dynamics.torsionalResistance << "\n";

    if (packet.dynamics.recoverySuggested)
        summary << "Recovery hint: strong thematic pivot detected; broaden retrieval and down-rank stale context.\n";

    summary << "\nTop context:\n";
    for (const auto& snippet : packet.snippets)
        summary << "- [" << snippet.category << "] " << snippet.title << " (" << juce::String(snippet.relevanceScore, 2)
                 << ")\n";

    return summary;
}
}
