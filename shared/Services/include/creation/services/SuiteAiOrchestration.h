#pragma once

#include <creation/services/SuiteAiProviderRuntime.h>

namespace creation::services
{
enum class SuiteAiCapability : std::uint64_t
{
    textChat            = 1ull << 0,
    codeReasoning       = 1ull << 1,
    visionInput         = 1ull << 2,
    audioTranscription  = 1ull << 3,
    audioGeneration     = 1ull << 4,
    imageGeneration     = 1ull << 5,
    videoGeneration     = 1ull << 6,
    toolCalling         = 1ull << 7,
    structuredOutput    = 1ull << 8,
    streaming           = 1ull << 9
};

enum class SuiteAiHealthState
{
    unknown,
    healthy,
    degraded,
    unavailable,
    coolingDown
};

enum class SuiteAiCostTier
{
    cheap,
    standard,
    premium
};

struct SuiteAiCapabilitySet
{
    std::uint64_t bits = 0;

    void add(SuiteAiCapability capability) noexcept;
    bool contains(SuiteAiCapability capability) const noexcept;
    bool containsAll(const SuiteAiCapabilitySet& other) const noexcept;
    bool isEmpty() const noexcept;
    juce::String toDisplayString() const;

    static SuiteAiCapabilitySet fromList(std::initializer_list<SuiteAiCapability> capabilities) noexcept;
};

struct SuiteAiThrottlePolicy
{
    int maxConcurrentRequests = 2;
    int burstLimit = 4;
    int cooldownMs = 5000;
};

struct SuiteAiRetryPolicy
{
    int maxRetries = 2;
    int initialBackoffMs = 250;
    bool retryOnNetworkFailure = true;
    bool retryOnRateLimit = true;
    bool retryOnAuthFailure = false;
};

struct SuiteAiFallbackPolicy
{
    bool allowFallback = true;
    bool allowCrossProviderFallback = true;
    bool allowSameProviderFallback = true;
    int maxCandidates = 3;
};

struct SuiteAiBudgetPolicy
{
    bool enforceBudget = false;
    double maxEstimatedCostUsd = 0.0;
    bool preferLowerCost = true;
};

struct SuiteAiRequestDescriptor
{
    creation::assets::SuiteAppDomain appDomain = creation::assets::SuiteAppDomain::unknown;
    juce::String operationName;
    juce::String preferredAccountId;
    juce::String preferredProviderId;
    juce::String preferredModelName;
    bool preferLocalRuntime = false;
    SuiteAiCapabilitySet requiredCapabilities;
    SuiteAiThrottlePolicy throttlePolicy;
    SuiteAiRetryPolicy retryPolicy;
    SuiteAiFallbackPolicy fallbackPolicy;
    SuiteAiBudgetPolicy budgetPolicy;
};

struct SuiteAiProviderHealthSnapshot
{
    juce::String accountId;
    SuiteAiHealthState healthState = SuiteAiHealthState::unknown;
    int consecutiveFailures = 0;
    int observedRateLimitCount = 0;
    juce::int64 lastLatencyMs = 0;
    juce::Time cooldownUntil;
    juce::String lastError;

    bool isAvailableNow(juce::Time now = juce::Time::getCurrentTime()) const noexcept;
};

struct SuiteAiRouteCandidate
{
    juce::String accountId;
    juce::String providerId;
    juce::String providerDisplayName;
    juce::String modelName;
    juce::String baseUrl;
    SuiteAiCapabilitySet capabilities;
    SuiteAiHealthState healthState = SuiteAiHealthState::unknown;
    SuiteAiCostTier costTier = SuiteAiCostTier::standard;
    double estimatedCostUsd = 0.0;
    bool localRuntime = false;
    int priorityScore = 0;
    juce::String rationale;
};

struct SuiteAiRoutingDecision
{
    juce::Array<SuiteAiRouteCandidate> candidates;
    juce::StringArray blockedReasons;

    bool isRoutable() const noexcept;
    juce::String summary() const;
};

struct SuiteAiDiagnosticsEvent
{
    juce::String eventType;
    juce::String providerId;
    juce::String accountId;
    juce::String modelName;
    juce::String message;
    int retryCount = 0;
    bool fallbackUsed = false;
    juce::int64 queueDurationMs = 0;
    juce::int64 latencyMs = 0;
};

class SuiteAiOrchestrator final
{
public:
    static SuiteAiCapabilitySet capabilitySetForProfile(const SuiteAiProviderRuntimeProfile& profile) noexcept;
    static SuiteAiCostTier costTierForProfile(const SuiteAiProviderRuntimeProfile& profile) noexcept;
    static SuiteAiRoutingDecision planRoutes(const SuiteAiSettings& settings,
                                             const SuiteAiRequestDescriptor& request,
                                             const juce::Array<SuiteAiProviderHealthSnapshot>& healthSnapshots = {});
    static juce::String describeCandidate(const SuiteAiRouteCandidate& candidate);
};
}
