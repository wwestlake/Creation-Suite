#pragma once

#include <juce_core/juce_core.h>

#include "creation/assets/AssetCatalog.h"

namespace creation::assets
{
enum class SuiteAppDomain
{
    unknown,
    station,
    engine,
    movie,
    live,
    texture,
    modeler,
    developer,
    // Reserved for the suite's own root data (AI settings, auth session, etc.), stored
    // directly under the VFS root's "suite/" folder by services/VfsService::VfsProjectStore
    // -- not a project folder at all, so this domain value exists only to keep
    // SuiteAppDomain exhaustive elsewhere; it's never passed to listProjects/
    // listAllProjects/findProjectById.
    suite
};

struct ProjectManifest
{
    juce::String projectId;
    juce::String projectName;
    SuiteAppDomain appDomain = SuiteAppDomain::unknown;
    int schemaVersion = 1;
    int revision = 0;
    juce::String createdWithSuiteVersion;
    juce::String createdByAppVersion;
    juce::Time createdAt;
    juce::Time modifiedAt;
    AssetCatalog assetCatalog;
    juce::StringArray tags;
};

struct ProjectContainerPaths
{
    static constexpr const char* manifestPath = "Project/project-manifest.json";
    static constexpr const char* assetRoot = "Assets/";
    static constexpr const char* sourceAssetRoot = "Assets/Source/";
    static constexpr const char* derivedAssetRoot = "Assets/Derived/";
    static constexpr const char* metadataRoot = "Metadata/";
    static constexpr const char* exportsRoot = "Exports/";
};

juce::String toStorageToken(SuiteAppDomain domain);
SuiteAppDomain suiteAppDomainFromStorageToken(const juce::String& token);
juce::String toDisplayName(SuiteAppDomain domain);

juce::var toVar(const ProjectManifest& manifest);
bool fromVar(const juce::var& value, ProjectManifest& outManifest);
juce::String serializeManifest(const ProjectManifest& manifest, bool prettyPrint = true);
bool deserializeManifest(const juce::String& jsonText, ProjectManifest& outManifest, juce::String& errorMessage);
}
