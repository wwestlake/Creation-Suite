#include <creation/characters/CharacterSchemas.h>

#include <algorithm>

namespace creation::characters
{
namespace
{
juce::ValueTree toReferenceTree(const juce::Identifier& name, const AssetReference& reference)
{
    juce::ValueTree value(name);
    value.setProperty("assetId", reference.assetId, nullptr);
    value.setProperty("versionId", reference.versionId, nullptr);
    return value;
}

AssetReference fromReferenceTree(const juce::ValueTree& value)
{
    return { value.getProperty("assetId").toString(), value.getProperty("versionId").toString() };
}

void appendReferenceList(juce::ValueTree& root, const juce::Identifier& listName,
                         const juce::Identifier& entryName, const std::vector<AssetReference>& values)
{
    juce::ValueTree list(listName);
    for (const auto& value : values)
        list.addChild(toReferenceTree(entryName, value), -1, nullptr);
    root.addChild(list, -1, nullptr);
}

void readReferenceList(const juce::ValueTree& root, const juce::Identifier& listName,
                       const juce::Identifier& entryName, std::vector<AssetReference>& values)
{
    if (const auto list = root.getChildWithName(listName); list.isValid())
        for (const auto entry : list)
            if (entry.hasType(entryName)) values.push_back(fromReferenceTree(entry));
}

void appendStringList(juce::ValueTree& root, const juce::Identifier& listName,
                      const juce::Identifier& entryName, const std::vector<juce::String>& values)
{
    juce::ValueTree list(listName);
    for (const auto& value : values) { juce::ValueTree entry(entryName); entry.setProperty("id", value, nullptr); list.addChild(entry, -1, nullptr); }
    root.addChild(list, -1, nullptr);
}

void readStringList(const juce::ValueTree& root, const juce::Identifier& listName,
                    const juce::Identifier& entryName, std::vector<juce::String>& values)
{
    if (const auto list = root.getChildWithName(listName); list.isValid())
        for (const auto entry : list)
            if (entry.hasType(entryName)) values.push_back(entry.getProperty("id").toString());
}
}

juce::String toStorageToken(DocumentKind kind)
{
    switch (kind)
    {
        case DocumentKind::baseType: return "baseType";
        case DocumentKind::recipe: return "recipe";
        case DocumentKind::definition: return "definition";
        case DocumentKind::creatorPolicy: return "creatorPolicy";
        case DocumentKind::instance: return "instance";
        case DocumentKind::roster: return "roster";
        case DocumentKind::garment: return "garment";
    }
    return "baseType";
}

DocumentKind documentKindFromStorageToken(const juce::String& value)
{
    if (value == "recipe") return DocumentKind::recipe;
    if (value == "definition") return DocumentKind::definition;
    if (value == "creatorPolicy") return DocumentKind::creatorPolicy;
    if (value == "instance") return DocumentKind::instance;
    if (value == "roster") return DocumentKind::roster;
    if (value == "garment") return DocumentKind::garment;
    return DocumentKind::baseType;
}

juce::String toStorageToken(GarmentFitMode mode)
{
    return mode == GarmentFitMode::socketAttached ? "socketAttached" : "conformingSkinned";
}

GarmentFitMode garmentFitModeFromStorageToken(const juce::String& value)
{
    return value == "socketAttached" ? GarmentFitMode::socketAttached : GarmentFitMode::conformingSkinned;
}

juce::ValueTree CharacterDocument::toValueTree() const
{
    juce::ValueTree root("CharacterDocument");
    root.setProperty("schemaVersion", currentSchemaVersion, nullptr);
    root.setProperty("kind", toStorageToken(kind), nullptr);
    root.setProperty("id", id, nullptr);
    root.setProperty("displayName", displayName, nullptr);
    root.setProperty("description", description, nullptr);
    root.setProperty("category", category, nullptr);
    root.setProperty("maximumRosterSlots", maximumRosterSlots, nullptr);
    root.addChild(toReferenceTree("BaseCharacter", baseCharacter), -1, nullptr);
    root.addChild(toReferenceTree("Skeleton", skeleton), -1, nullptr);
    root.addChild(toReferenceTree("VisualDefinition", visualDefinition), -1, nullptr);
    root.addChild(toReferenceTree("SourceAsset", sourceAsset), -1, nullptr);
    root.addChild(toReferenceTree("AnimationSet", animationSet), -1, nullptr);
    root.addChild(toReferenceTree("ValidationReport", validationReport), -1, nullptr);
    root.setProperty("topologyId", topologyId, nullptr);
    root.setProperty("canonicalSkeletonId", canonicalSkeletonId, nullptr);
    root.setProperty("canonicalSkeletonVersion", canonicalSkeletonVersion, nullptr);
    root.setProperty("buildHash", buildHash, nullptr);
    root.setProperty("buildStatus", buildStatus, nullptr);
    root.setProperty("garmentSlot", garmentSlot, nullptr);
    root.setProperty("garmentFitMode", toStorageToken(garmentFitMode), nullptr);
    root.setProperty("measuredHeightMeters", measuredHeightMeters, nullptr);
    root.setProperty("capsuleRadiusMeters", capsuleRadiusMeters, nullptr);
    root.setProperty("capsuleHalfHeightMeters", capsuleHalfHeightMeters, nullptr);

    juce::ValueTree parametersTree("Parameters");
    for (const auto& [key, value] : parameters) parametersTree.setProperty(key, value, nullptr);
    root.addChild(parametersTree, -1, nullptr);
    juce::ValueTree minimums("ParameterMinimums");
    for (const auto& [key, value] : parameterMinimums) minimums.setProperty(key, value, nullptr);
    root.addChild(minimums, -1, nullptr);
    juce::ValueTree maximums("ParameterMaximums");
    for (const auto& [key, value] : parameterMaximums) maximums.setProperty(key, value, nullptr);
    root.addChild(maximums, -1, nullptr);
    appendReferenceList(root, "Garments", "Garment", garments);
    appendReferenceList(root, "Materials", "Material", materials);
    appendReferenceList(root, "Equipment", "Equipment", equipment);
    appendStringList(root, "AllowedBases", "Base", allowedBaseIds);
    appendStringList(root, "AllowedDefinitions", "Definition", allowedDefinitionIds);
    appendStringList(root, "AllowedGarments", "GarmentId", allowedGarmentIds);
    appendStringList(root, "ExposedParameters", "Parameter", exposedParameterIds);
    appendStringList(root, "BodyCoverage", "Region", bodyCoverageRegions);
    juce::ValueTree anchorsTree("RigAnchors");
    for (const auto& anchor : anchors) { juce::ValueTree entry("Anchor"); entry.setProperty("id", anchor.id, nullptr); entry.setProperty("target", anchor.target, nullptr); entry.setProperty("role", anchor.role, nullptr); anchorsTree.addChild(entry, -1, nullptr); }
    root.addChild(anchorsTree, -1, nullptr);
    return root;
}

juce::String CharacterDocument::serialize() const
{
    if (auto xml = toValueTree().createXml()) return xml->toString();
    return {};
}

bool CharacterDocument::fromValueTree(const juce::ValueTree& value, CharacterDocument& result, juce::String& error)
{
    if (!value.hasType("CharacterDocument")) { error = "Not a Character Document."; return false; }
    const int version = static_cast<int>(value.getProperty("schemaVersion", 0));
    if (version <= 0 || version > currentSchemaVersion) { error = "Unsupported Character Document schema version."; return false; }
    result = {};
    result.kind = documentKindFromStorageToken(value.getProperty("kind").toString());
    result.id = value.getProperty("id").toString().trim();
    result.displayName = value.getProperty("displayName").toString();
    result.description = value.getProperty("description").toString();
    result.category = value.getProperty("category").toString();
    result.maximumRosterSlots = std::max(1, static_cast<int>(value.getProperty("maximumRosterSlots", 1)));
    if (result.id.isEmpty()) { error = "Character Document needs a stable id."; return false; }
    result.baseCharacter = fromReferenceTree(value.getChildWithName("BaseCharacter"));
    result.skeleton = fromReferenceTree(value.getChildWithName("Skeleton"));
    result.visualDefinition = fromReferenceTree(value.getChildWithName("VisualDefinition"));
    result.sourceAsset = fromReferenceTree(value.getChildWithName("SourceAsset"));
    result.animationSet = fromReferenceTree(value.getChildWithName("AnimationSet"));
    result.validationReport = fromReferenceTree(value.getChildWithName("ValidationReport"));
    result.topologyId = value.getProperty("topologyId").toString();
    result.canonicalSkeletonId = value.getProperty("canonicalSkeletonId").toString();
    result.canonicalSkeletonVersion = value.getProperty("canonicalSkeletonVersion").toString();
    result.buildHash = value.getProperty("buildHash").toString();
    result.buildStatus = value.getProperty("buildStatus").toString();
    result.garmentSlot = value.getProperty("garmentSlot").toString();
    result.garmentFitMode = garmentFitModeFromStorageToken(value.getProperty("garmentFitMode").toString());
    result.measuredHeightMeters = static_cast<float>(value.getProperty("measuredHeightMeters", 0.0));
    result.capsuleRadiusMeters = static_cast<float>(value.getProperty("capsuleRadiusMeters", 0.0));
    result.capsuleHalfHeightMeters = static_cast<float>(value.getProperty("capsuleHalfHeightMeters", 0.0));
    if (const auto parametersTree = value.getChildWithName("Parameters"); parametersTree.isValid())
        for (int index = 0; index < parametersTree.getNumProperties(); ++index)
            result.parameters.emplace(parametersTree.getPropertyName(index).toString(), static_cast<float>(parametersTree.getProperty(parametersTree.getPropertyName(index))));
    if (const auto minimums = value.getChildWithName("ParameterMinimums"); minimums.isValid())
        for (int index = 0; index < minimums.getNumProperties(); ++index) result.parameterMinimums.emplace(minimums.getPropertyName(index).toString(), static_cast<float>(minimums.getProperty(minimums.getPropertyName(index))));
    if (const auto maximums = value.getChildWithName("ParameterMaximums"); maximums.isValid())
        for (int index = 0; index < maximums.getNumProperties(); ++index) result.parameterMaximums.emplace(maximums.getPropertyName(index).toString(), static_cast<float>(maximums.getProperty(maximums.getPropertyName(index))));
    readReferenceList(value, "Garments", "Garment", result.garments);
    readReferenceList(value, "Materials", "Material", result.materials);
    readReferenceList(value, "Equipment", "Equipment", result.equipment);
    readStringList(value, "AllowedBases", "Base", result.allowedBaseIds);
    readStringList(value, "AllowedDefinitions", "Definition", result.allowedDefinitionIds);
    readStringList(value, "AllowedGarments", "GarmentId", result.allowedGarmentIds);
    readStringList(value, "ExposedParameters", "Parameter", result.exposedParameterIds);
    readStringList(value, "BodyCoverage", "Region", result.bodyCoverageRegions);
    if (const auto anchorsTree = value.getChildWithName("RigAnchors"); anchorsTree.isValid())
        for (const auto entry : anchorsTree) if (entry.hasType("Anchor")) result.anchors.push_back({ entry.getProperty("id").toString(), entry.getProperty("target").toString(), entry.getProperty("role").toString() });
    return true;
}

bool CharacterDocument::deserialize(const juce::String& text, CharacterDocument& result, juce::String& error)
{
    auto xml = juce::XmlDocument::parse(text);
    if (xml == nullptr) { error = "Character Document XML is invalid."; return false; }
    return fromValueTree(juce::ValueTree::fromXml(*xml), result, error);
}

} // namespace creation::characters
