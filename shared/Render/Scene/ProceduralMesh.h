#pragma once

#include <vector>

#include <juce_core/juce_core.h>
#include <juce_opengl/juce_opengl.h>

#include "Render/Scene/Vertex.h"

namespace ce {

// Generates a smooth-shaded UV sphere (unit radius, centered at origin)
// with correct per-vertex normals — a simple stand-in test mesh until
// M4 brings in real imported geometry.
void GenerateUVSphere(int rings, int segments, std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

// Generates a unit cube (extents -0.5..0.5, centered at origin) with
// 24 vertices (4 per face, not shared across faces) so each face gets a
// correct flat normal and its own 0..1 UV range — the standard box mesh
// layout, same one BoxTextured.gltf itself uses.
void GenerateCube(std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

// Generates a unit cylinder (radius 0.5, extents -0.5..0.5 along Y, centered
// at origin) — same -0.5..0.5 convention as GenerateCube, so both primitives
// scale identically via a plain per-axis size factor. Wall vertices carry
// smooth radial normals (GenerateUVSphere's ring style); the two end caps
// get their own flat-normal vertex sets (GenerateCube's per-face technique),
// since a cap vertex and a wall vertex at the same position need different
// normals.
void GenerateCylinder(int segments, std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

// Extrudes a T-slot aluminum-extrusion-style cross-section (a square bar
// with one straight-sided channel notched into each of its four faces,
// centered along Y, extruded along Y by lengthMeters) centered at the
// origin. NOT a general polygon-with-holes extruder, and NOT a true two-
// stage narrow-mouth/wide-body T channel -- a single straight notch per
// side, built by grid-decomposing the solid into non-overlapping boxes (see
// the .cpp) rather than hand-tracing a boundary polygon. All parameters are
// meters, already converted from a ProfileSpec's millimeter fields by the
// caller. slotOpeningWidthMeters is accepted for forward compatibility with
// a future true-T-profile generator but is not yet used by this one.
void GenerateTSlotExtrusion(float outerWidthMeters, float slotOpeningWidthMeters,
                            float slotChannelWidthMeters, float slotDepthMeters,
                            float lengthMeters, std::vector<Vertex>& outVertices,
                            std::vector<GLuint>& outIndices);

// Approximates a DIN top-hat rail's cross-section (IEC/EN 60715 -- e.g.
// TS35x7.5, TS35x15) as a flat base plate plus two raised lengthwise edge
// ridges near where real modules' spring clips grip -- NOT the true rolled/
// curled-edge profile. Same fidelity discipline GenerateTSlotExtrusion
// already established for its own grid-decomposed notch topology: a
// recognizable, correctly-DIMENSIONED stand-in, not manufacturing-grade.
// Centered at the origin, extruded along Y by lengthMeters -- matches
// GenerateTSlotExtrusion's own axis convention exactly, which matters here:
// a module mounted on this rail shares the rail's own rotationDegrees
// verbatim, so rail and module must agree on which local axis is "along the
// length" (see creation::engineering::ConnectorSpec's dinModule doc comment
// for the module side).
void GenerateDinRailTopHat(float outerWidthMeters, float depthMeters, float lengthMeters,
                           std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

// Generic connector hardware meshes (ConnectorSpec-backed scene objects),
// each centered at the origin so a pure translation places the mesh's
// bounding-box center at the object's position, same convention as every
// other generator in this file once its output is placed in the scene.
void GenerateConnectorCornerBracket(float sizeXMeters, float sizeYMeters, float sizeZMeters,
                                    std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);
void GenerateConnectorTNut(float sizeXMeters, float sizeYMeters, float sizeZMeters,
                           std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

// A single circular hole in a sketch profile, in the sketch's own 2D (u,v)
// frame -- meters, matching EngineerSceneModel::SketchHole exactly (kept as
// a separate plain struct here so shared/Render never depends on an
// app-level type).
struct SketchHoleDefinition
{
    juce::Point<float> centerUV;
    float diameterMeters;
};

// Extrudes an arbitrary simple polygon (straight-line boundary, any winding
// -- corrected internally to CCW) with zero or more circular holes into a
// flat-capped solid. Always generated in a fixed local (X=u, Y=v, Z=+-
// thickness/2) frame -- the caller (EngineerViewportComponent, via the
// object's model matrix) is what places/orients the result in the world;
// this function has no notion of which real-world plane the sketch was
// authored on. circleSegments controls each hole's roundness (24 is a
// reasonable default, matching typical GenerateCylinder caller values).
// Holes are merged into the boundary via the standard "bridge edge" ear-
// clipping-with-holes technique (see the .cpp for the concrete algorithm);
// self-intersecting input is not validated or rejected -- garbage in,
// garbage out for v1.
void GenerateExtrudedPolygonWithHoles(const std::vector<juce::Point<float>>& outerBoundaryUV,
                                      const std::vector<SketchHoleDefinition>& holes,
                                      float thicknessMeters, int circleSegments,
                                      std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

// Expands an 8-corner editable vertex cage (fixed bit-order: bit0=+X,
// bit1=+Y, bit2=+Z -- e.g. corner 0 = (-X,-Y,-Z), corner 7 = (+X,+Y,+Z))
// into a flat-shaded 24-vertex mesh, the same per-face vertex-duplication
// technique GenerateCube uses, except each face's normal is *computed* from
// the corners' actual current positions (cross product of two edges) rather
// than assumed axis-aligned -- so a cage already dragged away from a simple
// box still gets a reasonable flat-shaded look instead of a wrong hardcoded
// normal. Requires exactly 8 corners; does nothing otherwise.
void BuildFlatShadedMeshFromCage(const std::vector<juce::Vector3D<float>>& corners,
                                 std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices);

} // namespace ce
