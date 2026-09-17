#pragma once

#include <juce_core/juce_core.h>

#include "creation/engineering/SpecTypes.h"

namespace creation::engineering
{
// tSlotExtrusion: T-slot aluminum extrusion (see shared/Render's
// GenerateTSlotExtrusion). dinRailTopHat: IEC/EN 60715 DIN top-hat mounting
// rail (see GenerateDinRailTopHat) -- reuses this same struct's
// outerWidthMm/outerHeightMm for the rail's 35mm width/depth; the
// T-slot-specific fields below are simply unused for this kind, same
// "accepted but not consumed" precedent GenerateTSlotExtrusion already sets
// for slotOpeningWidthMm.
enum class ProfileKind
{
    tSlotExtrusion,
    dinRailTopHat
};

juce::String toStorageToken(ProfileKind kind);
ProfileKind profileKindFromStorageToken(const juce::String& token);

// A cross-section family a part can be extruded from, e.g. "2020" T-slot
// aluminum extrusion. All dimensional fields are millimeters, matching how
// extrusion catalogs publish them -- shared/Render's GenerateTSlotExtrusion
// converts to meters at generation time.
struct ProfileSpec
{
    juce::String id;
    juce::String displayName;
    ProfileKind kind = ProfileKind::tSlotExtrusion;
    juce::String familyName;
    float outerWidthMm = 0.0f;
    float outerHeightMm = 0.0f;
    float slotOpeningWidthMm = 0.0f;
    float slotChannelWidthMm = 0.0f;
    float slotDepthMm = 0.0f;
    float wallThicknessMm = 0.0f;
    float centerBoreDiameterMm = 0.0f;
    int slotsPerSide = 1;
    juce::String defaultMaterialId;
    SpecSource source;
};

juce::var toVar(const ProfileSpec& profile);
bool fromVar(const juce::var& value, ProfileSpec& outProfile);
}
