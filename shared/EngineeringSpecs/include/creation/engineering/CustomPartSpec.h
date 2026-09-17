#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

#include "creation/engineering/SpecTypes.h"

namespace creation::engineering
{
// Which of the 3 principal planes a CustomPartSpec's sketch was originally
// authored on -- stored purely for round-trip/future "re-edit this sketch"
// fidelity, same "informational only" precedent ConnectorSpec::
// compatibleFamilyName already establishes for a field that exists for
// humans/UI, not the mesh generator (ce::GenerateExtrudedPolygonWithHoles
// never consumes this -- it always generates in its own fixed local frame).
enum class SketchPlaneToken
{
    xy,
    xz,
    yz
};

juce::String toStorageToken(SketchPlaneToken plane);
SketchPlaneToken sketchPlaneTokenFromStorageToken(const juce::String& token);

// A single circular hole in a CustomPartSpec's sketch, in the sketch's own
// 2D (u,v) frame -- meters, matching EngineerSceneModel::SketchHole and
// shared/Render's ce::SketchHoleDefinition exactly (three separate plain
// structs, deliberately: EngineerSceneModel/shared/Render/shared/
// EngineeringSpecs never depend on each other's types).
struct CustomPartHole
{
    juce::Point<float> centerUV;
    float diameterMeters = 0.0f;
};

// A reusable, sketch-authored part -- the outcome of Creation Engineer's
// sketch/extrude workflow (a plate, bracket, or any other flat-extruded
// shape a user designed) saved once and placed as many independent
// SceneObject instances as needed, each referencing this record by id
// rather than duplicating geometry. Exclusively user-authored: no built-in
// seed content, unlike ProfileSpec/ConnectorSpec.
struct CustomPartSpec
{
    juce::String id;
    juce::String displayName;
    juce::String defaultMaterialId;
    SketchPlaneToken plane = SketchPlaneToken::xz;
    std::vector<juce::Point<float>> boundaryUV;
    std::vector<CustomPartHole> holes;
    float thicknessMeters = 0.0f;
    SpecSource source;
};

juce::var toVar(const CustomPartSpec& part);
bool fromVar(const juce::var& value, CustomPartSpec& outPart);
}
