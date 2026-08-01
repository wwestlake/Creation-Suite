#include <creation/services/SuiteAiOrchestration.h>

#include <algorithm>

namespace
{
using creation::services::SuiteAiCapability;
using creation::services::SuiteAiCapabilitySet;
using creation::services::SuiteAiCostTier;
using creation::services::SuiteAiHealthState;
using creation::services::SuiteAiProviderHealthSnapshot;
using creation::services::SuiteAiRequestDescriptor;
using creation::services::SuiteAiRouteCandidate;
using creation::services::SuiteAiRoutingDecision;
using creation::services::SuiteAiSettings;
using creation::services::SuiteAiSettingsResolver;
using creation::services::SuiteAiProviderRuntime;
using creation::services::SuiteAiProviderRuntimeProfile;

const SuiteAiProviderHealthSnapshot* findHealthSnapshot(const juce::Array<SuiteAiProviderHealthSnapshot>& snapshots,
                                                        const juce::String& accountId) noexcept
{
    for (const auto& snapshot : snapshots)
        if (snapshot.accountId == accountId)
            return std::addressof(snapshot);

    return nullptr;
}

int scoreHealth(SuiteAiHealthState state) noexcept
{
    switch (state)
    {
        case SuiteAiHealthState::healthy: return 80;
        case SuiteAiHealthState::degraded: return 20;
        case SuiteAiHealthState::unknown: return 5;
        case SuiteAiHealthState::coolingDown: return -250;
        case SuiteAiHealthState::unavailable: return -500;
    }

    return 0;
}

int scoreCostTier(SuiteAiCostTier tier) noexcept
{
    switch (tier)
    {
        case SuiteAiCostTier::cheap: return 40;
        case SuiteAiCostTier::standard: return 10;
        case SuiteAiCostTier::premium: return -20;
    }

    return 0;
}

juce::String capabilityName(SuiteAiCapability capability)
{
    switch (capability)
    {
        case SuiteAiCapability::textChat: return "text";
        case SuiteAiCapability::codeReasoning: return "code";
        case SuiteAiCapability::visionInput: return "vision";
        case SuiteAiCapability::audioTranscription: return "transcription";
        case SuiteAiCapability::audioGeneration: return "audio-gen";
        case SuiteAiCapability::imageGeneration: return "image-gen";
        case SuiteAiCapability::videoGeneration: return "video-gen";
        case SuiteAiCapability::toolCalling: return "tools";
        case SuiteAiCapability::structuredOutput: return "structured";
        case SuiteAiCapability::streaming: return "streaming";
    }

    return "unknown";
}

juce::String healthName(SuiteAiHealthState state)
{
    switch (state)
    {
        case SuiteAiHealthState::unknown: return "unknown";
        case SuiteAiHealthState::healthy: return "healthy";
        case SuiteAiHealthState::degraded: return "degraded";
        case SuiteAiHealthState::unavailable: return "unavailable";
        case SuiteAiHealthState::coolingDown: return "cooling-down";
    }

    return "unknown";
}

juce::String costTierName(SuiteAiCostTier tier)
{
    switch (tier)
    {
        case SuiteAiCostTier::cheap: return "cheap";
        case SuiteAiCostTier::standard: return "standard";
        case SuiteAiCostTier::premium: return "premium";
    }

    return "standard";
}

double estimateRequestCostUsd(const SuiteAiProviderRuntimeProfile& profile,
                              const SuiteAiRequestDescriptor& request,
                              SuiteAiCostTier tier) noexcept
{
    juce::ignoreUnused(profile);

    double estimate = 0.0;
    switch (tier)
    {
        case SuiteAiCostTier::cheap: estimate = 0.01; break;
        case SuiteAiCostTier::standard: estimate = 0.05; break;
        case SuiteAiCostTier::premium: estimate = 0.20; break;
    }

    if (request.requiredCapabilities.contains(SuiteAiCapability::visionInput))
        estimate += 0.03;
    if (request.requiredCapabilities.contains(SuiteAiCapability::audioTranscription))
        estimate += 0.02;
    if (request.requiredCapabilities.contains(SuiteAiCapability::audioGeneration))
        estimate += 0.06;
    if (request.requiredCapabilities.contains(SuiteAiCapability::imageGeneration))
        estimate += 0.08;
    if (request.requiredCapabilities.contains(SuiteAiCapability::videoGeneration))
        estimate += 0.25;
    if (request.requiredCapabilities.contains(SuiteAiCapability::toolCalling))
        estimate += 0.01;
    if (request.preferLocalRuntime && (profile.isOllamaStyle() || profile.providerId == "lm-studio"))
        estimate = 0.0;

    return estimate;
}

void applyFallbackPolicy(juce::Array<SuiteAiRouteCandidate>& candidates,
                         const creation::services::SuiteAiFallbackPolicy& policy)
{
    if (candidates.size() <= 1)
        return;

    if (! policy.allowFallback || (! policy.allowCrossProviderFallback && ! policy.allowSameProviderFallback))
    {
        candidates.removeRange(1, candidates.size() - 1);
        return;
    }

    const auto primaryProviderId = candidates.getFirst().providerId;
    for (int index = candidates.size(); --index > 0;)
    {
        const auto sameProvider = candidates.getReference(index).providerId == primaryProviderId;
        if (sameProvider && ! policy.allowSameProviderFallback)
            candidates.remove(index);
        else if (! sameProvider && ! policy.allowCrossProviderFallback)
            candidates.remove(index);
    }
}

bool candidateMatchesPreferredFamily(const creation::services::SuiteAiRouteCandidate& candidate,
                                     const creation::services::SuiteAiRequestDescriptor& request,
                                     const creation::services::SuiteAiSettings& settings) noexcept
{
    if (request.preferredAccountId.isNotEmpty())
        return candidate.accountId == request.preferredAccountId;

    if (request.preferredProviderId.isNotEmpty())
        return candidate.providerId == creation::services::SuiteAiProviderRuntime::normalizeProviderId(request.preferredProviderId);

    if (request.preferLocalRuntime)
        return candidate.localRuntime;

    if (const auto* appSelection = creation::services::SuiteAiSettingsResolver::findAppSelection(settings, request.appDomain))
        return appSelection->enabled && candidate.accountId == appSelection->accountId;

    return false;
}
}

namespace creation::services
{
void SuiteAiCapabilitySet::add(SuiteAiCapability capability) noexcept
{
    bits |= static_cast<std::uint64_t>(capability);
}

bool SuiteAiCapabilitySet::contains(SuiteAiCapability capability) const noexcept
{
    return (bits & static_cast<std::uint64_t>(capability)) != 0;
}

bool SuiteAiCapabilitySet::containsAll(const SuiteAiCapabilitySet& other) const noexcept
{
    return (bits & other.bits) == other.bits;
}

bool SuiteAiCapabilitySet::isEmpty() const noexcept
{
    return bits == 0;
}

juce::String SuiteAiCapabilitySet::toDisplayString() const
{
    juce::StringArray names;
    for (const auto capability : { SuiteAiCapability::textChat,
                                   SuiteAiCapability::codeReasoning,
                                   SuiteAiCapability::visionInput,
                                   SuiteAiCapability::audioTranscription,
                                   SuiteAiCapability::audioGeneration,
                                   SuiteAiCapability::imageGeneration,
                                   SuiteAiCapability::videoGeneration,
                                   SuiteAiCapability::toolCalling,
                                   SuiteAiCapability::structuredOutput,
                                   SuiteAiCapability::streaming })
    {
        if (contains(capability))
            names.add(capabilityName(capability));
    }

    return names.joinIntoString(", ");
}

SuiteAiCapabilitySet SuiteAiCapabilitySet::fromList(std::initializer_list<SuiteAiCapability> capabilities) noexcept
{
    SuiteAiCapabilitySet set;
    for (const auto capability : capabilities)
        set.add(capability);
    return set;
}

bool SuiteAiProviderHealthSnapshot::isAvailableNow(juce::Time now) const noexcept
{
    if (healthState == SuiteAiHealthState::unavailable)
        return false;

    if (healthState == SuiteAiHealthState::coolingDown && cooldownUntil > now)
        return false;

    return true;
}

bool SuiteAiRoutingDecision::isRoutable() const noexcept
{
    return ! candidates.isEmpty();
}

juce::String SuiteAiRoutingDecision::summary() const
{
    if (candidates.isEmpty())
        return blockedReasons.joinIntoString(" | ");

    juce::StringArray summaries;
    for (const auto& candidate : candidates)
        summaries.add(SuiteAiOrchestrator::describeCandidate(candidate));
    return summaries.joinIntoString(" | ");
}

SuiteAiCapabilitySet SuiteAiOrchestrator::capabilitySetForProfile(const SuiteAiProviderRuntimeProfile& profile) noexcept
{
    auto capabilities = SuiteAiCapabilitySet::fromList({
        SuiteAiCapability::textChat,
        SuiteAiCapability::codeReasoning,
        SuiteAiCapability::structuredOutput,
        SuiteAiCapability::streaming
    });

    if (! profile.isOllamaStyle())
    {
        capabilities.add(SuiteAiCapability::toolCalling);
        capabilities.add(SuiteAiCapability::visionInput);
    }

    if (profile.providerId == "openai" || profile.providerId == "google" || profile.providerId == "azure-openai")
    {
        capabilities.add(SuiteAiCapability::audioTranscription);
        capabilities.add(SuiteAiCapability::audioGeneration);
        capabilities.add(SuiteAiCapability::imageGeneration);
    }

    if (profile.providerId == "openai" || profile.providerId == "google")
        capabilities.add(SuiteAiCapability::videoGeneration);

    return capabilities;
}

SuiteAiCostTier SuiteAiOrchestrator::costTierForProfile(const SuiteAiProviderRuntimeProfile& profile) noexcept
{
    if (profile.isOllamaStyle())
        return SuiteAiCostTier::cheap;

    if (profile.providerId == "openrouter" || profile.providerId == "groq" || profile.providerId == "custom-openai")
        return SuiteAiCostTier::standard;

    return SuiteAiCostTier::premium;
}

SuiteAiRoutingDecision SuiteAiOrchestrator::planRoutes(const SuiteAiSettings& settings,
                                                       const SuiteAiRequestDescriptor& request,
                                                       const juce::Array<SuiteAiProviderHealthSnapshot>& healthSnapshots)
{
    SuiteAiRoutingDecision decision;

    juce::Array<juce::String> accountOrder;
    auto addAccountIfMissing = [&accountOrder](const juce::String& accountId)
    {
        if (accountId.isNotEmpty() && ! accountOrder.contains(accountId))
            accountOrder.add(accountId);
    };

    addAccountIfMissing(request.preferredAccountId);

    if (const auto* appSelection = SuiteAiSettingsResolver::findAppSelection(settings, request.appDomain))
        if (appSelection->enabled)
            addAccountIfMissing(appSelection->accountId);

    addAccountIfMissing(settings.defaultAccountId);

    for (const auto& account : settings.accounts)
        if (account.enabled)
            addAccountIfMissing(account.accountId);

    if (accountOrder.isEmpty())
    {
        decision.blockedReasons.add("No suite AI accounts are configured.");
        return decision;
    }

    for (const auto& accountId : accountOrder)
    {
        const auto* account = SuiteAiSettingsResolver::findAccountById(settings, accountId);
        if (account == nullptr)
        {
            decision.blockedReasons.add("Requested AI account '" + accountId + "' does not exist.");
            continue;
        }

        if (! account->enabled)
        {
            decision.blockedReasons.add("AI account '" + account->accountLabel + "' is disabled.");
            continue;
        }

        const auto profile = SuiteAiProviderRuntime::resolveProfile(account->providerId);
        const auto normalizedProviderId = SuiteAiProviderRuntime::normalizeProviderId(account->providerId);
        if (request.preferredProviderId.isNotEmpty()
            && normalizedProviderId != SuiteAiProviderRuntime::normalizeProviderId(request.preferredProviderId))
        {
            continue;
        }

        if (SuiteAiProviderRuntime::requiresApiKey(profile, account->apiKey))
        {
            decision.blockedReasons.add("AI account '" + account->accountLabel + "' is missing an API key.");
            continue;
        }

        const auto capabilities = capabilitySetForProfile(profile);
        if (! capabilities.containsAll(request.requiredCapabilities))
        {
            decision.blockedReasons.add("Provider '" + profile.displayName + "' is missing required capabilities: "
                                        + request.requiredCapabilities.toDisplayString());
            continue;
        }

        SuiteAiHealthState healthState = SuiteAiHealthState::unknown;
        juce::String healthNote;
        if (const auto* health = findHealthSnapshot(healthSnapshots, account->accountId))
        {
            healthState = health->healthState;
            if (! health->isAvailableNow())
            {
                decision.blockedReasons.add("AI account '" + account->accountLabel + "' is cooling down or unavailable.");
                continue;
            }

            if (health->lastError.isNotEmpty())
                healthNote = " last error: " + health->lastError;
        }

        SuiteAiRouteCandidate candidate;
        candidate.accountId = account->accountId;
        candidate.providerId = normalizedProviderId;
        candidate.providerDisplayName = profile.displayName;
        candidate.modelName = request.preferredModelName.isNotEmpty()
                                ? request.preferredModelName
                                : SuiteAiSettingsResolver::resolveModelNameForApp(settings, request.appDomain);

        if (candidate.modelName.isEmpty())
            candidate.modelName = account->modelName.isNotEmpty()
                                    ? account->modelName
                                    : SuiteAiProviderRuntime::defaultModelName(profile);

        candidate.baseUrl = SuiteAiProviderRuntime::normalizeBaseUrl(account->baseUrl, profile);
        candidate.capabilities = capabilities;
        candidate.healthState = healthState;
        candidate.costTier = costTierForProfile(profile);
        candidate.estimatedCostUsd = estimateRequestCostUsd(profile, request, candidate.costTier);
        candidate.localRuntime = profile.isOllamaStyle() || normalizedProviderId == "custom-openai";

        if (request.budgetPolicy.enforceBudget
            && request.budgetPolicy.maxEstimatedCostUsd > 0.0
            && candidate.estimatedCostUsd > request.budgetPolicy.maxEstimatedCostUsd)
        {
            decision.blockedReasons.add("AI account '" + account->accountLabel + "' exceeds the request budget (estimated $"
                                        + juce::String(candidate.estimatedCostUsd, 2) + ").");
            continue;
        }

        if (request.preferredAccountId == account->accountId)
            candidate.priorityScore += 500;
        else if (request.preferredProviderId.isNotEmpty() && candidate.providerId == SuiteAiProviderRuntime::normalizeProviderId(request.preferredProviderId))
            candidate.priorityScore += 250;
        else if (const auto* resolvedAccount = SuiteAiSettingsResolver::resolveAccountForApp(settings, request.appDomain))
            if (resolvedAccount->accountId == account->accountId)
                candidate.priorityScore += 200;

        candidate.priorityScore += scoreHealth(candidate.healthState);

        if (request.preferLocalRuntime)
            candidate.priorityScore += candidate.localRuntime ? 150 : -75;
        else if (candidate.localRuntime)
            candidate.priorityScore -= 15;

        if (request.budgetPolicy.preferLowerCost)
            candidate.priorityScore += scoreCostTier(candidate.costTier);

        candidate.rationale = candidate.providerDisplayName
                            + " (" + costTierName(candidate.costTier) + ", "
                            + healthName(candidate.healthState) + ", est $" + juce::String(candidate.estimatedCostUsd, 2) + ")";
        if (candidate.localRuntime)
            candidate.rationale << " local runtime";
        if (healthNote.isNotEmpty())
            candidate.rationale << healthNote;

        decision.candidates.add(candidate);
    }

    std::sort(decision.candidates.begin(),
              decision.candidates.end(),
              [](const SuiteAiRouteCandidate& left, const SuiteAiRouteCandidate& right)
              {
                  return left.priorityScore > right.priorityScore;
              });

    applyFallbackPolicy(decision.candidates, request.fallbackPolicy);

    if (request.fallbackPolicy.maxCandidates > 0 && decision.candidates.size() > request.fallbackPolicy.maxCandidates)
        decision.candidates.removeRange(request.fallbackPolicy.maxCandidates,
                                        decision.candidates.size() - request.fallbackPolicy.maxCandidates);

    if (! decision.candidates.isEmpty()
        && ! request.fallbackPolicy.allowCrossProviderFallback
        && ! candidateMatchesPreferredFamily(decision.candidates.getFirst(), request, settings))
    {
        decision.blockedReasons.add("Preferred AI route is unavailable and cross-provider fallback is disabled.");
        decision.candidates.clear();
    }

    if (decision.candidates.isEmpty() && decision.blockedReasons.isEmpty())
        decision.blockedReasons.add("No provider route matched the current suite AI policy.");

    return decision;
}

juce::String SuiteAiOrchestrator::describeCandidate(const SuiteAiRouteCandidate& candidate)
{
    return candidate.providerDisplayName
         + " / " + candidate.modelName
         + " / " + candidate.capabilities.toDisplayString()
         + " / est $" + juce::String(candidate.estimatedCostUsd, 2)
         + " / score " + juce::String(candidate.priorityScore)
         + " / " + candidate.rationale;
}
}
