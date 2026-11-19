#include "creation/assets/ProjectManifest.h"
#include <map>
#include <set>
#include <functional>

namespace creation::assets
{
bool mergeProjectManifest(const ProjectManifest& base, const ProjectManifest& edited,
                          const ProjectManifest& current, ProjectManifest& merged,
                          juce::String& error)
{
    if (base.projectId.isEmpty() || edited.projectId != base.projectId || current.projectId != base.projectId)
    {
        error = "Cannot merge different project identities.";
        return false;
    }
    std::function<bool(const juce::var&, const juce::var&)> same;
    same = [&same](const juce::var& a, const juce::var& b)
    {
        if (const auto* object = a.getDynamicObject())
        {
            const auto* other = b.getDynamicObject();
            if (other == nullptr || object->getProperties().size() != other->getProperties().size()) return false;
            for (const auto& property : object->getProperties())
                if (!other->hasProperty(property.name) || !same(property.value, other->getProperty(property.name))) return false;
            return true;
        }
        if (const auto* array = a.getArray())
        {
            const auto* other = b.getArray();
            if (other == nullptr || array->size() != other->size()) return false;
            for (int i = 0; i < array->size(); ++i) if (!same(array->getReference(i), other->getReference(i))) return false;
            return true;
        }
        return !b.isObject() && !b.isArray() && a == b;
    };
    auto result = toVar(current);
    const auto before = toVar(base), after = toVar(edited);
    for (const auto& field : before.getDynamicObject()->getProperties())
    {
        const auto key = field.name;
        if (key == juce::Identifier("assetCatalog") || key == juce::Identifier("revision")
            || key == juce::Identifier("modifiedAtMs") || same(before[key], after[key]))
            continue;
        if (!same(result[key], before[key]) && !same(result[key], after[key]))
        {
            error = "Project metadata changed in another application: " + key.toString() + ". Reopen before editing it again.";
            return false;
        }
        result.getDynamicObject()->setProperty(key, after[key]);
    }
    using Versions = std::map<juce::String, AssetDescriptor>;
    auto versions = [&error](const AssetCatalog& catalog, Versions& out)
    {
        for (const auto& asset : catalog.assets)
        {
            const auto key = asset.versionId.isNotEmpty() ? asset.versionId : asset.id + "|" + asset.logicalPath;
            if (asset.id.isEmpty() || !out.emplace(key, asset).second)
            {
                error = "Project catalog contains an invalid or duplicate asset version.";
                return false;
            }
        }
        return true;
    };
    Versions oldAssets, localAssets, remoteAssets;
    if (!versions(base.assetCatalog, oldAssets) || !versions(edited.assetCatalog, localAssets)
        || !versions(current.assetCatalog, remoteAssets)) return false;
    auto equal = [&same](const Versions& a, const Versions& b, const juce::String& key)
    {
        const auto x = a.find(key), y = b.find(key);
        return x == a.end() ? y == b.end() : y != b.end() && same(toVar(x->second), toVar(y->second));
    };
    std::set<juce::String> keys;
    for (const auto& [key, asset] : oldAssets) { juce::ignoreUnused(asset); keys.insert(key); }
    for (const auto& [key, asset] : localAssets) { juce::ignoreUnused(asset); keys.insert(key); }
    auto assets = current.assetCatalog;
    for (const auto& key : keys)
    {
        if (equal(oldAssets, localAssets, key)) continue;
        if (!equal(remoteAssets, oldAssets, key) && !equal(remoteAssets, localAssets, key))
        {
            error = "Asset version changed in another application: " + key + ". Reopen before editing it again.";
            return false;
        }
        for (int i = assets.assets.size(); --i >= 0;)
        {
            const auto& a = assets.assets.getReference(i);
            if ((a.versionId.isNotEmpty() ? a.versionId : a.id + "|" + a.logicalPath) == key)
                assets.assets.remove(i);
        }
        if (const auto found = localAssets.find(key); found != localAssets.end()) assets.assets.add(found->second);
    }
    result.getDynamicObject()->setProperty("assetCatalog", toVar(assets));
    ProjectManifest candidate;
    if (!fromVar(result, candidate)) { error = "Merged project metadata is invalid."; return false; }
    merged = std::move(candidate);
    return true;
}
}
