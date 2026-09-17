#include <creation/engineering/BuiltinSpecLoader.h>
#include <creation/engineering/SpecLibrary.h>

#include <juce_core/juce_core.h>

#include <iostream>

namespace
{
void fail(const juce::String& message)
{
    std::cerr << message << std::endl;
    throw std::runtime_error(message.toStdString());
}
}

int main()
{
    try
    {
        using namespace creation::engineering;

        // --- round-trip one record of each type through toVar/fromVar ------
        MaterialSpec material;
        material.id = "material:smoke";
        material.displayName = "Smoke Alloy";
        material.alloyDesignation = "0000-T0";
        material.densityKgM3 = 2700.0f;
        material.yieldStrengthMPa = 145.0f;
        material.ultimateTensileStrengthMPa = 186.0f;
        material.elasticModulusGPa = 68.3f;
        material.shearModulusGPa = 25.8f;
        material.poissonsRatio = 0.33f;
        material.source.kind = SpecSourceKind::userAuthored;
        material.source.manufacturer = "Smoke Corp";
        material.source.partNumber = "SM-1";

        MaterialSpec materialRoundTrip;
        if (!fromVar(toVar(material), materialRoundTrip))
            fail("MaterialSpec fromVar(toVar(...)) failed.");
        if (materialRoundTrip.id != material.id || materialRoundTrip.densityKgM3 != material.densityKgM3
            || materialRoundTrip.source.kind != SpecSourceKind::userAuthored
            || materialRoundTrip.source.manufacturer != "Smoke Corp")
            fail("MaterialSpec round trip lost data.");

        ProfileSpec profile;
        profile.id = "profile:smoke";
        profile.displayName = "Smoke Profile";
        profile.kind = ProfileKind::tSlotExtrusion;
        profile.familyName = "9090";
        profile.outerWidthMm = 90.0f;
        profile.outerHeightMm = 90.0f;
        profile.slotOpeningWidthMm = 10.0f;
        profile.slotChannelWidthMm = 15.0f;
        profile.slotDepthMm = 12.0f;
        profile.wallThicknessMm = 3.0f;
        profile.centerBoreDiameterMm = 8.0f;
        profile.slotsPerSide = 1;
        profile.defaultMaterialId = material.id;

        ProfileSpec profileRoundTrip;
        if (!fromVar(toVar(profile), profileRoundTrip))
            fail("ProfileSpec fromVar(toVar(...)) failed.");
        if (profileRoundTrip.familyName != "9090" || profileRoundTrip.outerWidthMm != 90.0f
            || profileRoundTrip.kind != ProfileKind::tSlotExtrusion)
            fail("ProfileSpec round trip lost data.");

        // profileKindFromStorageToken went from an always-tSlotExtrusion stub
        // to a real two-branch function in the DIN-rail phase -- the check
        // above never would have caught a regression there since it only
        // ever exercised the stub's single always-correct answer.
        ProfileSpec dinRailProfile;
        dinRailProfile.id = "profile:smoke-dinrail";
        dinRailProfile.displayName = "Smoke DIN Rail";
        dinRailProfile.kind = ProfileKind::dinRailTopHat;
        dinRailProfile.familyName = "TS35x7.5";
        dinRailProfile.outerWidthMm = 35.0f;
        dinRailProfile.outerHeightMm = 7.5f;

        ProfileSpec dinRailProfileRoundTrip;
        if (!fromVar(toVar(dinRailProfile), dinRailProfileRoundTrip))
            fail("DIN rail ProfileSpec fromVar(toVar(...)) failed.");
        if (dinRailProfileRoundTrip.kind != ProfileKind::dinRailTopHat
            || dinRailProfileRoundTrip.outerHeightMm != 7.5f)
            fail("DIN rail ProfileSpec round trip lost data.");

        ConnectorSpec connector;
        connector.id = "connector:smoke";
        connector.displayName = "Smoke Bracket";
        connector.kind = ConnectorKind::cornerBracket;
        connector.compatibleFamilyName = "9090";
        connector.sizeMmX = 90.0f;
        connector.sizeMmY = 90.0f;
        connector.sizeMmZ = 70.0f;

        ConnectorSpec connectorRoundTrip;
        if (!fromVar(toVar(connector), connectorRoundTrip))
            fail("ConnectorSpec fromVar(toVar(...)) failed.");
        if (connectorRoundTrip.sizeMmZ != 70.0f || connectorRoundTrip.kind != ConnectorKind::cornerBracket)
            fail("ConnectorSpec round trip lost data.");

        // --- SpecLibrary container round trip + merge ----------------------
        SpecLibrary library;
        library.materials.push_back(material);
        library.profiles.push_back(profile);
        library.connectors.push_back(connector);

        SpecLibrary libraryRoundTrip;
        if (!fromVar(toVar(library), libraryRoundTrip))
            fail("SpecLibrary fromVar(toVar(...)) failed.");
        if (libraryRoundTrip.materials.size() != 1 || libraryRoundTrip.profiles.size() != 1
            || libraryRoundTrip.connectors.size() != 1)
            fail("SpecLibrary round trip lost a record.");
        if (libraryRoundTrip.findMaterial(material.id) == nullptr)
            fail("SpecLibrary findMaterial failed after round trip.");

        SpecLibrary overlay;
        overlay.materials.push_back(material); // same id -- should be skipped
        MaterialSpec secondMaterial = material;
        secondMaterial.id = "material:smoke-2";
        overlay.materials.push_back(secondMaterial);

        libraryRoundTrip.merge(overlay);
        if (libraryRoundTrip.materials.size() != 2)
            fail("SpecLibrary::merge should skip colliding ids and append new ones.");

        // --- builtin JSON files parse and are non-empty ---------------------
#ifdef ENGINEERING_SPECS_DATA_DIR
        SpecLibrary builtins;
        juce::String loadError;
        if (!loadBuiltinSpecLibrary(juce::File(ENGINEERING_SPECS_DATA_DIR), builtins, loadError))
            fail("loadBuiltinSpecLibrary failed: " + loadError);
        if (builtins.materials.empty() || builtins.profiles.empty() || builtins.connectors.empty())
            fail("Builtin spec library loaded but one or more collections was empty.");
        if (builtins.findProfile("profile:tslot-2020") == nullptr)
            fail("Builtin spec library is missing the expected 2020 T-slot profile.");
        if (builtins.findProfile("profile:dinrail-ts35x7.5") == nullptr)
            fail("Builtin spec library is missing the expected TS35x7.5 DIN rail profile.");
        if (builtins.findConnector("connector:dinmodule-terminal-block") == nullptr)
            fail("Builtin spec library is missing the expected DIN terminal-block module.");
#endif

        std::cout << "EngineeringSpecsSmoke: all checks passed." << std::endl;
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "EngineeringSpecsSmoke failure: " << exception.what() << std::endl;
        return 1;
    }
}
