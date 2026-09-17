#include "creation/engineering/ProfileSpec.h"

namespace creation::engineering
{
juce::String toStorageToken(ProfileKind kind)
{
    switch (kind)
    {
        case ProfileKind::dinRailTopHat: return "dinRailTopHat";
        case ProfileKind::tSlotExtrusion: break;
    }

    return "tSlotExtrusion";
}

ProfileKind profileKindFromStorageToken(const juce::String& token)
{
    if (token.trim().equalsIgnoreCase("dinRailTopHat"))
        return ProfileKind::dinRailTopHat;

    return ProfileKind::tSlotExtrusion;
}

juce::var toVar(const ProfileSpec& profile)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("id", profile.id);
    object->setProperty("displayName", profile.displayName);
    object->setProperty("kind", toStorageToken(profile.kind));
    object->setProperty("familyName", profile.familyName);
    object->setProperty("outerWidthMm", profile.outerWidthMm);
    object->setProperty("outerHeightMm", profile.outerHeightMm);
    object->setProperty("slotOpeningWidthMm", profile.slotOpeningWidthMm);
    object->setProperty("slotChannelWidthMm", profile.slotChannelWidthMm);
    object->setProperty("slotDepthMm", profile.slotDepthMm);
    object->setProperty("wallThicknessMm", profile.wallThicknessMm);
    object->setProperty("centerBoreDiameterMm", profile.centerBoreDiameterMm);
    object->setProperty("slotsPerSide", profile.slotsPerSide);
    object->setProperty("defaultMaterialId", profile.defaultMaterialId);
    object->setProperty("source", toVar(profile.source));
    return juce::var(object);
}

bool fromVar(const juce::var& value, ProfileSpec& outProfile)
{
    const auto* object = value.getDynamicObject();
    if (object == nullptr)
        return false;

    outProfile.id = object->getProperty("id").toString();
    outProfile.displayName = object->getProperty("displayName").toString();
    outProfile.kind = profileKindFromStorageToken(object->getProperty("kind").toString());
    outProfile.familyName = object->getProperty("familyName").toString();
    outProfile.outerWidthMm = static_cast<float>(static_cast<double>(object->getProperty("outerWidthMm")));
    outProfile.outerHeightMm = static_cast<float>(static_cast<double>(object->getProperty("outerHeightMm")));
    outProfile.slotOpeningWidthMm = static_cast<float>(static_cast<double>(object->getProperty("slotOpeningWidthMm")));
    outProfile.slotChannelWidthMm = static_cast<float>(static_cast<double>(object->getProperty("slotChannelWidthMm")));
    outProfile.slotDepthMm = static_cast<float>(static_cast<double>(object->getProperty("slotDepthMm")));
    outProfile.wallThicknessMm = static_cast<float>(static_cast<double>(object->getProperty("wallThicknessMm")));
    outProfile.centerBoreDiameterMm = static_cast<float>(static_cast<double>(object->getProperty("centerBoreDiameterMm")));
    outProfile.slotsPerSide = static_cast<int>(object->getProperty("slotsPerSide"));
    outProfile.defaultMaterialId = object->getProperty("defaultMaterialId").toString();
    fromVar(object->getProperty("source"), outProfile.source);
    return true;
}
}
