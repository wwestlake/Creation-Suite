#pragma once

#include <juce_core/juce_core.h>

#include "creation/engineering/SpecTypes.h"

namespace creation::engineering
{
enum class ConnectorKind
{
    cornerBracket,
    tNut
};

juce::String toStorageToken(ConnectorKind kind);
ConnectorKind connectorKindFromStorageToken(const juce::String& token);

// A piece of hardware that joins two profile-library parts together, e.g. a
// corner bracket for 2020 T-slot. compatibleFamilyName matches
// ProfileSpec::familyName -- v1 placement is manual (see the parametric-part-
// libraries plan's Part H), this field is informational for the library
// browser only, not enforced by any snapping/constraint logic yet.
struct ConnectorSpec
{
    juce::String id;
    juce::String displayName;
    ConnectorKind kind = ConnectorKind::cornerBracket;
    juce::String compatibleFamilyName;
    float sizeMmX = 0.0f;
    float sizeMmY = 0.0f;
    float sizeMmZ = 0.0f;
    juce::String defaultMaterialId;
    SpecSource source;
};

juce::var toVar(const ConnectorSpec& connector);
bool fromVar(const juce::var& value, ConnectorSpec& outConnector);
}
