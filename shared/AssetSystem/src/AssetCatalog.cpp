#include "creation/assets/AssetCatalog.h"

namespace
{
juce::StringArray readStringArrayProperty(const juce::DynamicObject& object, const juce::Identifier& propertyName)
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
const AssetDescriptor* AssetCatalog::findById(const AssetId& assetId) const noexcept
{
    for (const auto& asset : assets)
        if (asset.id == assetId)
            return std::addressof(asset);

    return nullptr;
}

const AssetDescriptor* AssetCatalog::findByVersionId(const AssetVersionId& versionId) const noexcept
{
    for (const auto& asset : assets)
        if (asset.versionId == versionId)
            return std::addressof(asset);

    return nullptr;
}

juce::Array<AssetDescriptor> AssetCatalog::findAllVersions(const AssetId& assetId) const
{
    juce::Array<AssetDescriptor> versions;
    for (const auto& asset : assets)
        if (asset.id == assetId)
            versions.add(asset);

    std::sort(versions.begin(), versions.end(), [](const AssetDescriptor& left, const AssetDescriptor& right)
    {
        if (left.revision != right.revision)
            return left.revision > right.revision;
        return left.modifiedAt > right.modifiedAt;
    });

    return versions;
}

juce::Array<AssetDescriptor> AssetCatalog::query(const AssetQuery& queryDefinition) const
{
    juce::Array<AssetDescriptor> matches;

    for (const auto& asset : assets)
    {
        if (queryDefinition.kind.has_value() && asset.kind != *queryDefinition.kind)
            continue;

        if (queryDefinition.logicalPathPrefix.isNotEmpty()
            && ! asset.logicalPath.startsWithIgnoreCase(queryDefinition.logicalPathPrefix))
            continue;

        if (queryDefinition.searchText.isNotEmpty())
        {
            const auto haystack = asset.displayName + "\n" + asset.description + "\n" + asset.category;
            if (! haystack.containsIgnoreCase(queryDefinition.searchText))
                continue;
        }

        bool missingTag = false;
        for (const auto& tag : queryDefinition.tags)
        {
            if (! asset.tags.contains(tag))
            {
                missingTag = true;
                break;
            }
        }

        if (! missingTag)
            matches.add(asset);
    }

    return matches;
}

juce::var toVar(const AssetDescriptor& descriptor)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", descriptor.id);
    object->setProperty("versionId", descriptor.versionId);
    object->setProperty("originalAssetId", descriptor.originalAssetId);
    object->setProperty("derivedFromVersionId", descriptor.derivedFromVersionId);
    object->setProperty("displayName", descriptor.displayName);
    object->setProperty("kind", toStorageToken(descriptor.kind));
    object->setProperty("category", descriptor.category);
    object->setProperty("description", descriptor.description);
    object->setProperty("mediaType", descriptor.mediaType);
    object->setProperty("logicalPath", descriptor.logicalPath);
    object->setProperty("sourceApp", descriptor.sourceApp);
    object->setProperty("sourceTool", descriptor.sourceTool);
    object->setProperty("sourceAssetId", descriptor.sourceAssetId);
    object->setProperty("sourceVersionId", descriptor.sourceVersionId);
    object->setProperty("importerId", descriptor.importerId);
    object->setProperty("importerVersion", descriptor.importerVersion);
    object->setProperty("importSettings", descriptor.importSettings);
    object->setProperty("version", descriptor.version);
    object->setProperty("fileSizeBytes", static_cast<int64_t>(descriptor.fileSizeBytes));
    object->setProperty("revision", descriptor.revision);
    object->setProperty("createdAtMs", descriptor.createdAt.toMilliseconds());
    object->setProperty("modifiedAtMs", descriptor.modifiedAt.toMilliseconds());

    juce::Array<juce::var> tags;
    for (const auto& tag : descriptor.tags)
        tags.add(tag);
    object->setProperty("tags", tags);

    return juce::var(object);
}

bool fromVar(const juce::var& value, AssetDescriptor& outDescriptor)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outDescriptor.id = object->getProperty("id").toString();
    outDescriptor.versionId = object->getProperty("versionId").toString();
    outDescriptor.originalAssetId = object->getProperty("originalAssetId").toString();
    outDescriptor.derivedFromVersionId = object->getProperty("derivedFromVersionId").toString();
    outDescriptor.displayName = object->getProperty("displayName").toString();
    outDescriptor.kind = assetKindFromStorageToken(object->getProperty("kind").toString());
    outDescriptor.category = object->getProperty("category").toString();
    outDescriptor.description = object->getProperty("description").toString();
    outDescriptor.mediaType = object->getProperty("mediaType").toString();
    outDescriptor.logicalPath = object->getProperty("logicalPath").toString();
    outDescriptor.sourceApp = object->getProperty("sourceApp").toString();
    outDescriptor.sourceTool = object->getProperty("sourceTool").toString();
    outDescriptor.sourceAssetId = object->getProperty("sourceAssetId").toString();
    outDescriptor.sourceVersionId = object->getProperty("sourceVersionId").toString();
    outDescriptor.importerId = object->getProperty("importerId").toString();
    outDescriptor.importerVersion = object->getProperty("importerVersion").toString();
    outDescriptor.importSettings = object->getProperty("importSettings").toString();
    outDescriptor.version = object->getProperty("version").toString();
    outDescriptor.fileSizeBytes = static_cast<int64_t>(object->getProperty("fileSizeBytes"));
    outDescriptor.revision = static_cast<int>(object->getProperty("revision"));
    outDescriptor.createdAt = juce::Time(static_cast<int64_t>(object->getProperty("createdAtMs")));
    outDescriptor.modifiedAt = juce::Time(static_cast<int64_t>(object->getProperty("modifiedAtMs")));
    outDescriptor.tags = readStringArrayProperty(*object, "tags");
    return true;
}

juce::var toVar(const AssetCatalog& catalog)
{
    juce::Array<juce::var> values;
    for (const auto& asset : catalog.assets)
        values.add(toVar(asset));
    return values;
}

bool fromVar(const juce::var& value, AssetCatalog& outCatalog)
{
    const auto* array = value.getArray();
    if (array == nullptr)
        return false;

    outCatalog.assets.clearQuick();
    for (const auto& entry : *array)
    {
        AssetDescriptor descriptor;
        if (! fromVar(entry, descriptor))
            return false;
        outCatalog.assets.add(descriptor);
    }

    return true;
}
}
