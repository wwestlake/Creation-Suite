#include "Render/Scene/CursorGizmoRenderer.h"

using namespace juce::gl;

namespace ce {

namespace {
struct LineVertex {
    float position[3];
};
} // namespace

void CursorGizmoRenderer::Build(juce::Vector3D<float> center, float armLengthMeters) {
    const LineVertex vertices[6] = {
        { { center.x - armLengthMeters, center.y, center.z } }, { { center.x + armLengthMeters, center.y, center.z } }, // X
        { { center.x, center.y - armLengthMeters, center.z } }, { { center.x, center.y + armLengthMeters, center.z } }, // Y
        { { center.x, center.y, center.z - armLengthMeters } }, { { center.x, center.y, center.z + armLengthMeters } }, // Z
    };

    vertexBuffer_.Upload(GL_ARRAY_BUFFER, vertices, sizeof(vertices));

    vertexArray_.Bind();
    vertexBuffer_.Bind(GL_ARRAY_BUFFER);
    vertexArray_.SetAttribute(0, 3, sizeof(LineVertex), 0);
    gl::VertexArray::Unbind();
}

void CursorGizmoRenderer::Draw(int axisIndex) {
    vertexArray_.Bind();
    glDrawArrays(GL_LINES, axisIndex * 2, 2);
    gl::VertexArray::Unbind();
}

} // namespace ce
