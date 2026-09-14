#pragma once

#include <vector>

#include <creation/assets/AssetTypes.h>
#include <creation/assets/ProjectSession.h>

#include <creation/characters/CharacterSchemas.h>

namespace creation::characters
{

// The one persistence boundary for CharacterSystem documents. Callers never
// manufacture VFS paths or catalog descriptors by hand.
class CharacterAssetStore final
{
public:
    static bool save(assets::ProjectSession& session, const CharacterDocument& document,
                     assets::AssetDescriptor& savedAsset, juce::String& error);
    static bool loadAll(const assets::ProjectSession& session, std::vector<CharacterDocument>& documents,
                        juce::String& error);
};

} // namespace creation::characters
