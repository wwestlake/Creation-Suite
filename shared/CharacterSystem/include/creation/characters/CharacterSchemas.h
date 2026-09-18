#pragma once

#include <map>
#include <vector>

#include <juce_data_structures/juce_data_structures.h>

namespace creation::characters
{

enum class DocumentKind { baseType, recipe, definition, creatorPolicy, instance, roster, garment };
enum class GarmentFitMode { conformingSkinned, socketAttached };

struct AssetReference final
{
    juce::String assetId;
    juce::String versionId;
};

struct RigAnchor final
{
    juce::String id;
    juce::String target;
    juce::String role;
};

struct CharacterDocument final
{
    static constexpr int currentSchemaVersion = 2;

    DocumentKind kind = DocumentKind::baseType;
    juce::String id;
    juce::String displayName;
    juce::String description;
    juce::String category;
    AssetReference baseCharacter;
    AssetReference skeleton;
    AssetReference visualDefinition;
    AssetReference sourceAsset;
    AssetReference animationSet;
    AssetReference validationReport;
    std::map<juce::String, float> parameters;
    std::map<juce::String, float> parameterMinimums;
    std::map<juce::String, float> parameterMaximums;
    std::vector<AssetReference> garments;
    std::vector<AssetReference> materials;
    std::vector<AssetReference> equipment;
    std::vector<RigAnchor> anchors;
    std::vector<juce::String> allowedBaseIds;
    std::vector<juce::String> allowedDefinitionIds;
    std::vector<juce::String> allowedGarmentIds;
    std::vector<juce::String> exposedParameterIds;
    std::vector<juce::String> bodyCoverageRegions;
    juce::String topologyId;
    juce::String canonicalSkeletonId;
    juce::String canonicalSkeletonVersion;
    juce::String buildHash;
    juce::String buildStatus;
    juce::String garmentSlot;
    GarmentFitMode garmentFitMode = GarmentFitMode::conformingSkinned;
    float measuredHeightMeters = 0.0f;
    float capsuleRadiusMeters = 0.0f;
    float capsuleHalfHeightMeters = 0.0f;
    int maximumRosterSlots = 1;

    [[nodiscard]] juce::ValueTree toValueTree() const;
    [[nodiscard]] juce::String serialize() const;
    static bool fromValueTree(const juce::ValueTree& value, CharacterDocument& result, juce::String& error);
    static bool deserialize(const juce::String& text, CharacterDocument& result, juce::String& error);
};

[[nodiscard]] juce::String toStorageToken(DocumentKind kind);
[[nodiscard]] DocumentKind documentKindFromStorageToken(const juce::String& value);
[[nodiscard]] juce::String toStorageToken(GarmentFitMode mode);
[[nodiscard]] GarmentFitMode garmentFitModeFromStorageToken(const juce::String& value);

} // namespace creation::characters
