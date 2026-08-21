#pragma once

#include <juce_core/juce_core.h>

#include "creation/engineering/SpecTypes.h"

namespace creation::engineering
{
// Mechanical/physical properties for one named alloy or material. Fields use
// the units engineering material datasheets normally publish in (kg/m^3,
// MPa, GPa), so authoring a new entry from a datasheet is a direct
// field-for-field copy, not a unit-conversion exercise.
struct MaterialSpec
{
    juce::String id;
    juce::String displayName;
    juce::String alloyDesignation;
    float densityKgM3 = 0.0f;
    float yieldStrengthMPa = 0.0f;
    float ultimateTensileStrengthMPa = 0.0f;
    float elasticModulusGPa = 0.0f;
    float shearModulusGPa = 0.0f;
    float poissonsRatio = 0.0f;
    SpecSource source;
};

juce::var toVar(const MaterialSpec& material);
bool fromVar(const juce::var& value, MaterialSpec& outMaterial);
}
