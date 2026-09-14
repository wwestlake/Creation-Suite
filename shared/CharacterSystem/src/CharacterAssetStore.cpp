#include <creation/characters/CharacterAssetStore.h>

#include <creation/assets/ProjectAssetService.h>
#include <creation/assets/ProjectManifest.h>

namespace creation::characters
{
namespace
{
juce::String slugify(const juce::String& value)
{
    juce::String result;
    for (const auto character : value)
    {
        if (juce::CharacterFunctions::isLetterOrDigit(character)) result << juce::CharacterFunctions::toLowerCase(character);
        else if (character == ' ' || character == '-' || character == '_') result << '-';
    }
    return result.isEmpty() ? juce::Uuid().toString().replaceCharacter('-', '_') : result;
}
}

bool CharacterAssetStore::save(assets::ProjectSession& session, const CharacterDocument& document,
                               assets::AssetDescriptor& savedAsset, juce::String& error)
{
    if (!session.isValid()) { error = "Open a Suite project before saving character data."; return false; }
    if (document.id.trim().isEmpty()) { error = "Character Document needs a stable id."; return false; }

    const auto text = document.serialize();
    if (text.isEmpty()) { error = "Could not serialize Character Document."; return false; }
    const juce::MemoryBlock data(text.toRawUTF8(), text.getNumBytesAsUTF8());

    assets::ProjectAssetService::ImportOptions options;
    options.kind = assets::AssetKind::character;
    options.category = document.category.isNotEmpty() ? document.category : "Character/" + toStorageToken(document.kind);
    options.displayName = document.displayName.isNotEmpty() ? document.displayName : document.id;
    options.description = document.description;
    options.logicalPath = "Characters/" + toStorageToken(document.kind) + "/" + slugify(document.id) + ".character.xml";
    options.mediaType = "application/x-djehuti-character-document";
    options.sourceApp = "Djehuti Character System";
    options.sourceTool = "Character Creator";
    options.derivationKind = assets::AssetDerivationKind::referential;

    if (!assets::ProjectAssetService::saveGeneratedAsset(session, data, options, savedAsset, error)) return false;
    return session.commit(error);
}

bool CharacterAssetStore::loadAll(const assets::ProjectSession& session, std::vector<CharacterDocument>& documents,
                                  juce::String& error)
{
    documents.clear();
    assets::AssetQuery characterQuery;
    characterQuery.kind = assets::AssetKind::character;
    for (const auto& asset : session.getManifest().assetCatalog.query(characterQuery))
    {
        juce::MemoryBlock data;
        if (!session.readEntry(asset.logicalPath, data)) { error = "Could not read character asset: " + asset.logicalPath; return false; }
        CharacterDocument document;
        if (!CharacterDocument::deserialize(data.toString(), document, error))
        {
            error = "Character asset '" + asset.displayName + "' is invalid: " + error;
            return false;
        }
        documents.push_back(std::move(document));
    }
    return true;
}

} // namespace creation::characters
