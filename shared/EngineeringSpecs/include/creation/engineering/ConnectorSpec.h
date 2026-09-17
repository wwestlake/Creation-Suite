#pragma once

#include <juce_core/juce_core.h>

#include "creation/engineering/SpecTypes.h"

namespace creation::engineering
{
enum class ConnectorKind
{
    cornerBracket,
    tNut,
    dinModule
};

juce::String toStorageToken(ConnectorKind kind);
ConnectorKind connectorKindFromStorageToken(const juce::String& token);

// A piece of hardware that joins two profile-library parts together, e.g. a
// corner bracket for 2020 T-slot -- or, for ConnectorKind::dinModule, a
// panel device (terminal block, breaker, relay, enclosure) that snaps onto a
// DIN rail (ProfileKind::dinRailTopHat) in a contiguous strip. compatibleFamilyName matches
// ProfileSpec::familyName -- placement is manual for cornerBracket/tNut (see
// the parametric-part-libraries plan's Part H), this field is informational
// for the library browser only, not enforced by any snapping/constraint
// logic yet (including for dinModule -- compatibleFamilyName is not checked
// against the target rail at snap time either, see the DIN-rail plan's
// Risks).
//
// For ConnectorKind::dinModule specifically: sizeMmY is the module's width
// ALONG the rail (what determines snap spacing -- see
// EngineerLibraryComponent::snapSelectedConnectorToSelectedRail), sizeMmX is
// its extent across the rail's 35mm face, sizeMmZ is how far it projects
// away from the rail. This is deliberate, not arbitrary: GenerateDinRailTopHat
// and GenerateTSlotExtrusion both extrude along local Y, and a mounted
// module reuses its rail's rotationDegrees verbatim (no extra compensating
// rotation) -- putting the along-rail dimension on Y is what keeps both
// objects' "length" axes aligned in world space under that shared rotation,
// with zero change needed to GenerateConnectorTNut or renderConnectorObject
// (dinModule reuses GenerateConnectorTNut's plain box mesh as-is).
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
