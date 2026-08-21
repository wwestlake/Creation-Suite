#version 410 core

in vec3 vNormal;

uniform vec4 uBaseColour;
uniform vec3 uLightDirection;

out vec4 fragColour;

void main() {
    vec3 normal = normalize(vNormal);
    float diffuse = max(dot(normal, normalize(-uLightDirection)), 0.0);
    vec3 shaded = uBaseColour.rgb * (0.35 + diffuse * 0.65);
    fragColour = vec4(shaded, uBaseColour.a);
}
