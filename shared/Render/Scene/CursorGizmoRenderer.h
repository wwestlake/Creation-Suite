#pragma once

#include <juce_core/juce_core.h>
#include <juce_opengl/juce_opengl.h>

#include "Render/GL/Buffer.h"
#include "Render/GL/VertexArray.h"

namespace ce {

// A small 3-axis crosshair marking Creation Engineer's placement cursor
// (the insertion point new objects spawn at) -- position-only GL_LINES
// geometry, same shape/precedent as GridRenderer (not ce::Mesh: that class's
// Draw() is hardcoded to GL_TRIANGLES with a position+normal+uv layout, an
// unrelated vertex format for a line list). Each axis is its own 2-vertex
// segment so the caller can draw/colour X/Y/Z separately (the standard
// red/green/blue convention), unlike GridRenderer's single uniform-colour
// Draw().
class CursorGizmoRenderer final {
public:
    // Rebuilds the 3 axis segments centered at `center`, each spanning
    // +/-armLengthMeters along its axis. Cheap, but still only call this
    // when the cursor position actually changes (a discrete click/edit
    // event), not every frame -- same "rebuild lazily on state change"
    // discipline EngineerViewportComponent's per-object mesh caches already
    // follow. Needs a live GL context, same rule as GridRenderer::Build.
    void Build(juce::Vector3D<float> center, float armLengthMeters);

    // axisIndex: 0=X, 1=Y, 2=Z.
    void Draw(int axisIndex);

private:
    gl::Buffer vertexBuffer_;
    gl::VertexArray vertexArray_;
};

} // namespace ce
