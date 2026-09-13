#include <creation/assets/AssetTypes.h>
#include <creation/assets/ProjectSession.h>
#include <creation/characters/CharacterAssetStore.h>
#include <creation/characters/CharacterSchemas.h>
#include <creation/suite/SuiteSettings.h>
#include <creation/suite/SuiteStoragePaths.h>

#include <iostream>

namespace
{
[[noreturn]] void fail(const juce::String& message)
{
    throw std::runtime_error(message.toStdString());
}
}

int main()
{
    try
    {
        using namespace creation::characters;

        CharacterDocument recipe;
        recipe.kind = DocumentKind::recipe;
        recipe.id = "dave-young-worker";
        recipe.displayName = "Dave, Young Worker";
        recipe.description = "A test recipe for the first character creator slice.";
        recipe.category = "Character/Recipe";
        recipe.baseCharacter = { "dave-base", "version-base" };
        recipe.skeleton = { "humanoid-canonical", "version-rig" };
        recipe.visualDefinition = { "dave-visual", "version-visual" };
        recipe.parameters.emplace("height", 0.55f);
        recipe.parameters.emplace("muscularity", 0.35f);
        recipe.parameterMinimums.emplace("height", 0.35f);
        recipe.parameterMaximums.emplace("height", 0.80f);
        recipe.garments.push_back({ "dave-shirt", "version-shirt" });
        recipe.garments.push_back({ "dave-boots", "version-boots" });
        recipe.materials.push_back({ "dave-skin", "version-skin" });
        recipe.equipment.push_back({ "dave-hammer", "version-hammer" });
        recipe.topologyId = "dave-humanoid-topology-v1";
        recipe.canonicalSkeletonId = "djehuti-humanoid";
        recipe.canonicalSkeletonVersion = "1";
        recipe.measuredHeightMeters = 2.0f;
        recipe.capsuleRadiusMeters = 0.31f;
        recipe.capsuleHalfHeightMeters = 0.91f;
        recipe.anchors.push_back({ "head", "Head", "camera" });

        CharacterDocument restored;
        juce::String error;
        if (!CharacterDocument::deserialize(recipe.serialize(), restored, error)) fail(error);
        if (restored.kind != DocumentKind::recipe || restored.id != recipe.id ||
            restored.parameters.size() != 2 || restored.garments.size() != 2 ||
            restored.materials.size() != 1 || restored.equipment.size() != 1 ||
            restored.anchors.size() != 1 || restored.baseCharacter.assetId != "dave-base" ||
            restored.canonicalSkeletonId != "djehuti-humanoid" ||
            restored.measuredHeightMeters != 2.0f || restored.capsuleRadiusMeters != 0.31f)
            fail("Character recipe round trip lost data.");

        CharacterDocument policy;
        policy.kind = DocumentKind::creatorPolicy;
        policy.id = "starter-policy";
        policy.allowedBaseIds = { "dave-base", "robot-base" };
        policy.exposedParameterIds = { "height", "muscularity", "hair-style" };
        policy.maximumRosterSlots = 3;
        if (!CharacterDocument::deserialize(policy.serialize(), restored, error)) fail(error);
        if (restored.maximumRosterSlots != 3 || restored.allowedBaseIds.size() != 2 || restored.exposedParameterIds.size() != 3)
            fail("Character creator policy round trip lost data.");

        if (creation::assets::assetKindFromStorageToken("character") != creation::assets::AssetKind::character ||
            creation::assets::toStorageToken(creation::assets::AssetKind::character) != "character")
            fail("Character asset kind does not round trip.");

        const auto settings = creation::suite::SuiteSettingsStore().load(error);
        if (error.isNotEmpty()) fail("Could not load suite settings: " + error);

        creation::assets::ProjectSession session;
        if (!creation::assets::ProjectSession::createNew(settings,
                                                         creation::assets::SuiteAppDomain::engine,
                                                         "Character System Smoke",
                                                         "0.1.0",
                                                         "0.1.0",
                                                         session,
                                                         error))
            fail("Could not create smoke project: " + error);

        const auto projectId = session.getProjectId();
        creation::assets::AssetDescriptor savedRecipe;
        if (!CharacterAssetStore::save(session, recipe, savedRecipe, error))
            fail("Could not save character recipe: " + error);
        if (savedRecipe.kind != creation::assets::AssetKind::character ||
            !savedRecipe.logicalPath.startsWith("Characters/recipe/"))
            fail("Character recipe was not saved as a character asset.");
        session.close();

        creation::assets::ProjectSession reopened;
        if (!creation::assets::ProjectSession::open(settings, projectId, reopened, error))
            fail("Could not reopen smoke project: " + error);
        std::vector<CharacterDocument> documents;
        if (!CharacterAssetStore::loadAll(reopened, documents, error))
            fail("Could not reload character recipe: " + error);
        if (documents.size() != 1 || documents.front().id != recipe.id)
        {
            const auto& catalog = reopened.getManifest().assetCatalog.assets;
            juce::String details = "Reopened project did not retain the character recipe. Catalog contains "
                                   + juce::String(catalog.size()) + " asset(s): ";
            for (const auto& asset : catalog)
                details << "[" << asset.logicalPath << ", "
                        << creation::assets::toStorageToken(asset.kind) << "] ";
            fail(details);
        }
        reopened.close();

        creation::suite::getProjectContainerDirectory(settings).getChildFile(projectId).deleteRecursively();
        creation::suite::getMaterializedFilesDirectory(settings, projectId).deleteRecursively();

        std::cout << "CharacterSystemSmoke passed.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "CharacterSystemSmoke failure: " << exception.what() << '\n';
        return 1;
    }
}
