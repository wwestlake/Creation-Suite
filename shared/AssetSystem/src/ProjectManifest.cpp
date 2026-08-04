#include "creation/assets/ProjectManifest.h"

namespace
{
juce::StringArray readStringArray(const juce::DynamicObject& object, const juce::Identifier& propertyName)
{
    juce::StringArray values;
    if (const auto* array = object.getProperty(propertyName).getArray())
        for (const auto& value : *array)
            values.add(value.toString());
    return values;
}
}

namespace creation::assets
{
juce::String toStorageToken(SuiteAppDomain domain)
{
    switch (domain)
    {
        case SuiteAppDomain::station: return "station";
        case SuiteAppDomain::engine: return "engine";
        case SuiteAppDomain::movie: return "movie";
        case SuiteAppDomain::live: return "live";
        case SuiteAppDomain::texture: return "texture";
        case SuiteAppDomain::modeler: return "modeler";
        case SuiteAppDomain::suite: return "suite";
        case SuiteAppDomain::unknown: break;
    }

    return "unknown";
}

SuiteAppDomain suiteAppDomainFromStorageToken(const juce::String& token)
{
    const auto normalized = token.trim().toLowerCase();
    if (normalized == "station") return SuiteAppDomain::station;
    if (normalized == "engine") return SuiteAppDomain::engine;
    if (normalized == "movie") return SuiteAppDomain::movie;
    if (normalized == "live") return SuiteAppDomain::live;
    if (normalized == "texture") return SuiteAppDomain::texture;
    if (normalized == "modeler") return SuiteAppDomain::modeler;
    if (normalized == "suite") return SuiteAppDomain::suite;
    return SuiteAppDomain::unknown;
}

juce::String toDisplayName(SuiteAppDomain domain)
{
    switch (domain)
    {
        case SuiteAppDomain::station: return "Creation Station";
        case SuiteAppDomain::engine: return "Creation Engine";
        case SuiteAppDomain::movie: return "Creation Movie";
        case SuiteAppDomain::live: return "Creation Live";
        case SuiteAppDomain::texture: return "Creation Texture";
        case SuiteAppDomain::modeler: return "Creation Modeler";
        case SuiteAppDomain::suite: return "Creation Suite";
        case SuiteAppDomain::unknown: break;
    }

    return "Unknown";
}

juce::var toVar(const ProjectManifest& manifest)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("projectId", manifest.projectId);
    object->setProperty("projectName", manifest.projectName);
    object->setProperty("appDomain", toStorageToken(manifest.appDomain));
    object->setProperty("schemaVersion", manifest.schemaVersion);
    object->setProperty("revision", manifest.revision);
    object->setProperty("createdWithSuiteVersion", manifest.createdWithSuiteVersion);
    object->setProperty("createdByAppVersion", manifest.createdByAppVersion);
    object->setProperty("createdAtMs", manifest.createdAt.toMilliseconds());
    object->setProperty("modifiedAtMs", manifest.modifiedAt.toMilliseconds());
    object->setProperty("assetCatalog", toVar(manifest.assetCatalog));

    juce::Array<juce::var> tags;
    for (const auto& tag : manifest.tags)
        tags.add(tag);
    object->setProperty("tags", tags);

    return juce::var(object);
}

bool fromVar(const juce::var& value, ProjectManifest& outManifest)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outManifest.projectId = object->getProperty("projectId").toString();
    outManifest.projectName = object->getProperty("projectName").toString();
    outManifest.appDomain = suiteAppDomainFromStorageToken(object->getProperty("appDomain").toString());
    outManifest.schemaVersion = static_cast<int>(object->getProperty("schemaVersion"));
    outManifest.revision = static_cast<int>(object->getProperty("revision"));
    outManifest.createdWithSuiteVersion = object->getProperty("createdWithSuiteVersion").toString();
    outManifest.createdByAppVersion = object->getProperty("createdByAppVersion").toString();
    outManifest.createdAt = juce::Time(static_cast<int64_t>(object->getProperty("createdAtMs")));
    outManifest.modifiedAt = juce::Time(static_cast<int64_t>(object->getProperty("modifiedAtMs")));
    outManifest.tags = readStringArray(*object, "tags");

    AssetCatalog catalog;
    if (! fromVar(object->getProperty("assetCatalog"), catalog))
        return false;
    outManifest.assetCatalog = std::move(catalog);
    return true;
}

juce::String serializeManifest(const ProjectManifest& manifest, bool prettyPrint)
{
    return juce::JSON::toString(toVar(manifest), prettyPrint);
}

bool deserializeManifest(const juce::String& jsonText, ProjectManifest& outManifest, juce::String& errorMessage)
{
    const auto parsed = juce::JSON::parse(jsonText);
    if (parsed.isVoid())
    {
        errorMessage = "Could not parse the project manifest JSON.";
        return false;
    }

    if (! fromVar(parsed, outManifest))
    {
        errorMessage = "Project manifest JSON did not match the shared schema.";
        return false;
    }

    return true;
}
}
