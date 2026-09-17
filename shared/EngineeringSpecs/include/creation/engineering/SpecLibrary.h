#pragma once

#include <juce_core/juce_core.h>
#include <vector>

#include "creation/engineering/ConnectorSpec.h"
#include "creation/engineering/CustomPartSpec.h"
#include "creation/engineering/MaterialSpec.h"
#include "creation/engineering/ProfileSpec.h"

namespace creation::engineering
{
// The full set of profile/material/connector records available to Engineer's
// library browser and viewport. Built-in generic content (BuiltinSpecLoader)
// and a user's own saved entries (persisted separately, see
// EngineerLibraryComponent) are loaded into two SpecLibrary instances and
// combined with merge(), so the two sources never entangle on disk even
// though they're indistinguishable once loaded (aside from each record's own
// SpecSource tag).
struct SpecLibrary
{
    std::vector<MaterialSpec> materials;
    std::vector<ProfileSpec> profiles;
    std::vector<ConnectorSpec> connectors;
    // Exclusively user-authored (see CustomPartSpec's doc comment) -- no
    // builtin seed data, unlike the three vectors above.
    std::vector<CustomPartSpec> customParts;

    const MaterialSpec* findMaterial(const juce::String& id) const noexcept;
    const ProfileSpec* findProfile(const juce::String& id) const noexcept;
    const ConnectorSpec* findConnector(const juce::String& id) const noexcept;
    const CustomPartSpec* findCustomPart(const juce::String& id) const noexcept;

    // Appends every record from `overlay` whose id does not already exist in
    // this library. A colliding id is treated as a data-entry bug (the
    // authoring UI generates fresh juce::Uuid ids specifically to avoid this)
    // -- the colliding overlay record is simply skipped rather than surfaced
    // as an error here.
    void merge(const SpecLibrary& overlay);
};

juce::var toVar(const SpecLibrary& library);
bool fromVar(const juce::var& value, SpecLibrary& outLibrary);
}
