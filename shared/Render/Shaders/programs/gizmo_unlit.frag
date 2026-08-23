#version 410 core

// Flat, unlit colour -- deliberately no lighting math at all. The view-cube
// gizmo is a fixed orientation widget, not scene geometry: running it
// through engineer_lit's ambient+diffuse floor made shadowed faces (normal
// facing away from uLightDirection) render at nearly the same dark value as
// the gizmo's own background panel, so the cube visually disappeared into
// its backing square. Pairs with the existing generic programs/unlit.vert
// (its vNormal output is simply unused here).

uniform vec4 uBaseColour;

out vec4 fragColour;

void main() {
    fragColour = uBaseColour;
}
