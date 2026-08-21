#include "Render/Scene/ProceduralMesh.h"

#include <algorithm>
#include <cmath>

namespace ce {

namespace {
struct BoxVec3 {
    float x, y, z;
};

// Emits one axis-aligned box's 6 faces (24 vertices, 12 triangles) as flat-
// normal quads -- the exact per-face vertex-duplication technique GenerateCube
// already uses (n/u/v axis triples, "BL,BR,TL / TR,TL,BR" winding), just
// generalized from a fixed 0.5 half-extent to arbitrary per-axis half-extents
// and an arbitrary center. Positive non-uniform scaling never flips winding,
// so this inherits GenerateCube's already-correct CCW-from-outside guarantee.
void AppendBox(std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices, BoxVec3 center, BoxVec3 halfExtent) {
    struct Face {
        BoxVec3 n, u, v;
    };
    static const Face faces[6] = {
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },   // +Z
        { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } }, // -Z
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },  // +X
        { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },  // -X
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },  // +Y
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },  // -Y
    };

    const auto scale = [&halfExtent](BoxVec3 axis) -> BoxVec3 {
        return { axis.x * halfExtent.x, axis.y * halfExtent.y, axis.z * halfExtent.z };
    };
    const auto add = [](BoxVec3 a, BoxVec3 b, float s = 1.0f) -> BoxVec3 {
        return { a.x + b.x * s, a.y + b.y * s, a.z + b.z * s };
    };

    for (const auto& face : faces) {
        const auto faceCenter = add(center, scale(face.n));
        const auto u = scale(face.u);
        const auto v = scale(face.v);
        const BoxVec3 corners[4] = {
            add(add(faceCenter, u, -1.0f), v, -1.0f), // bottom-left
            add(add(faceCenter, u, 1.0f), v, -1.0f),  // bottom-right
            add(add(faceCenter, u, -1.0f), v, 1.0f),  // top-left
            add(add(faceCenter, u, 1.0f), v, 1.0f),   // top-right
        };
        const float uvs[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };

        const auto baseIndex = static_cast<GLuint>(outVertices.size());
        for (int i = 0; i < 4; ++i) {
            Vertex vertex{};
            vertex.position[0] = corners[i].x;
            vertex.position[1] = corners[i].y;
            vertex.position[2] = corners[i].z;
            vertex.normal[0] = face.n.x;
            vertex.normal[1] = face.n.y;
            vertex.normal[2] = face.n.z;
            vertex.uv[0] = uvs[i][0];
            vertex.uv[1] = uvs[i][1];
            outVertices.push_back(vertex);
        }

        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 1);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 3);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 1);
    }
}
} // namespace

void GenerateUVSphere(int rings, int segments, std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const float pi = juce::MathConstants<float>::pi;

    for (int ring = 0; ring <= rings; ++ring) {
        const float phi = pi * static_cast<float>(ring) / static_cast<float>(rings);
        const float y = std::cos(phi);
        const float ringRadius = std::sin(phi);

        for (int segment = 0; segment <= segments; ++segment) {
            const float theta = 2.0f * pi * static_cast<float>(segment) / static_cast<float>(segments);
            const float x = ringRadius * std::cos(theta);
            const float z = ringRadius * std::sin(theta);

            // Unit sphere: position and outward normal are the same vector.
            Vertex vertex{};
            vertex.position[0] = x;
            vertex.position[1] = y;
            vertex.position[2] = z;
            vertex.normal[0] = x;
            vertex.normal[1] = y;
            vertex.normal[2] = z;
            vertex.uv[0] = static_cast<float>(segment) / static_cast<float>(segments);
            vertex.uv[1] = static_cast<float>(ring) / static_cast<float>(rings);
            outVertices.push_back(vertex);
        }
    }

    const int rowStride = segments + 1;
    for (int ring = 0; ring < rings; ++ring) {
        for (int segment = 0; segment < segments; ++segment) {
            const GLuint a = static_cast<GLuint>(ring * rowStride + segment);
            const GLuint b = static_cast<GLuint>(a + rowStride);

            outIndices.push_back(a);
            outIndices.push_back(b);
            outIndices.push_back(a + 1);

            outIndices.push_back(a + 1);
            outIndices.push_back(b);
            outIndices.push_back(b + 1);
        }
    }
}

namespace {
struct Vec3 {
    float x, y, z;
};

Vec3 Add(Vec3 a, Vec3 b, float bScale) {
    return { a.x + b.x * bScale, a.y + b.y * bScale, a.z + b.z * bScale };
}
} // namespace

void GenerateCube(std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    // Per face: outward normal N, and in-face tangent axes U/V chosen so
    // U x V == N — that guarantees consistent CCW winding (viewed from
    // outside) across all six faces from one shared index pattern below,
    // rather than having to special-case winding per face.
    struct Face {
        Vec3 n, u, v;
    };
    const Face faces[6] = {
        { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } },   // +Z
        { { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 } }, // -Z
        { { 1, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 } },  // +X
        { { -1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 } },  // -X
        { { 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 } },  // +Y
        { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } },  // -Y
    };

    for (const auto& face : faces) {
        const Vec3 center = { face.n.x * 0.5f, face.n.y * 0.5f, face.n.z * 0.5f };
        const Vec3 corners[4] = {
            Add(Add(center, face.u, -0.5f), face.v, -0.5f), // bottom-left
            Add(Add(center, face.u, 0.5f), face.v, -0.5f),  // bottom-right
            Add(Add(center, face.u, -0.5f), face.v, 0.5f),  // top-left
            Add(Add(center, face.u, 0.5f), face.v, 0.5f),   // top-right
        };
        const float uvs[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };

        const auto baseIndex = static_cast<GLuint>(outVertices.size());
        for (int i = 0; i < 4; ++i) {
            Vertex vertex{};
            vertex.position[0] = corners[i].x;
            vertex.position[1] = corners[i].y;
            vertex.position[2] = corners[i].z;
            vertex.normal[0] = face.n.x;
            vertex.normal[1] = face.n.y;
            vertex.normal[2] = face.n.z;
            vertex.uv[0] = uvs[i][0];
            vertex.uv[1] = uvs[i][1];
            outVertices.push_back(vertex);
        }

        // BL, BR, TL / TR, TL, BR — CCW given U x V == N (see Face table above).
        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 1);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 3);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 1);
    }
}

void GenerateCylinder(int segments, std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const float pi = juce::MathConstants<float>::pi;
    constexpr float radius = 0.5f;
    constexpr float halfHeight = 0.5f;

    const auto wallBase = static_cast<GLuint>(outVertices.size());
    for (int i = 0; i <= segments; ++i) {
        const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        const float x = std::cos(theta);
        const float z = std::sin(theta);
        const float u = static_cast<float>(i) / static_cast<float>(segments);

        Vertex top{};
        top.position[0] = x * radius;
        top.position[1] = halfHeight;
        top.position[2] = z * radius;
        top.normal[0] = x;
        top.normal[2] = z;
        top.uv[0] = u;
        top.uv[1] = 1.0f;
        outVertices.push_back(top);

        Vertex bottom{};
        bottom.position[0] = x * radius;
        bottom.position[1] = -halfHeight;
        bottom.position[2] = z * radius;
        bottom.normal[0] = x;
        bottom.normal[2] = z;
        bottom.uv[0] = u;
        bottom.uv[1] = 0.0f;
        outVertices.push_back(bottom);
    }

    for (int i = 0; i < segments; ++i) {
        const GLuint topA = wallBase + static_cast<GLuint>(i * 2);
        const GLuint bottomA = topA + 1;
        const GLuint topB = wallBase + static_cast<GLuint>((i + 1) * 2);
        const GLuint bottomB = topB + 1;

        outIndices.push_back(topA);
        outIndices.push_back(bottomA);
        outIndices.push_back(topB);
        outIndices.push_back(topB);
        outIndices.push_back(bottomA);
        outIndices.push_back(bottomB);
    }

    const auto buildCap = [&](float y, float normalY) {
        const auto centerIndex = static_cast<GLuint>(outVertices.size());
        Vertex center{};
        center.position[1] = y;
        center.normal[1] = normalY;
        center.uv[0] = 0.5f;
        center.uv[1] = 0.5f;
        outVertices.push_back(center);

        const auto rimBase = static_cast<GLuint>(outVertices.size());
        for (int i = 0; i <= segments; ++i) {
            const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
            const float x = std::cos(theta);
            const float z = std::sin(theta);

            Vertex rim{};
            rim.position[0] = x * radius;
            rim.position[1] = y;
            rim.position[2] = z * radius;
            rim.normal[1] = normalY;
            rim.uv[0] = x * 0.5f + 0.5f;
            rim.uv[1] = z * 0.5f + 0.5f;
            outVertices.push_back(rim);
        }

        for (int i = 0; i < segments; ++i) {
            const GLuint a = rimBase + static_cast<GLuint>(i);
            const GLuint b = rimBase + static_cast<GLuint>(i + 1);
            if (normalY > 0.0f) {
                outIndices.push_back(centerIndex);
                outIndices.push_back(a);
                outIndices.push_back(b);
            } else {
                outIndices.push_back(centerIndex);
                outIndices.push_back(b);
                outIndices.push_back(a);
            }
        }
    };

    buildCap(halfHeight, 1.0f);
    buildCap(-halfHeight, -1.0f);
}

void GenerateTSlotExtrusion(float outerWidthMeters, float slotOpeningWidthMeters,
                            float slotChannelWidthMeters, float slotDepthMeters,
                            float lengthMeters, std::vector<Vertex>& outVertices,
                            std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();
    juce::ignoreUnused(slotOpeningWidthMeters);

    const float halfWidth = outerWidthMeters * 0.5f;
    const float halfLength = lengthMeters * 0.5f;
    const float channelHalfWidth = slotChannelWidthMeters * 0.5f;
    const float depth = juce::jlimit(0.0f, halfWidth - 0.0001f, slotDepthMeters);
    const float innerEdge = halfWidth - depth;

    // Grid-decompose "square minus 4 centered channel notches" into non-
    // overlapping boxes (a rasterized boolean difference) rather than hand-
    // tracing one boundary polygon -- see the header comment for why.
    std::vector<float> xBoundaries{ -halfWidth, -innerEdge, -channelHalfWidth,
                                    channelHalfWidth, innerEdge, halfWidth };
    std::vector<float> zBoundaries = xBoundaries; // square cross-section.
    std::sort(xBoundaries.begin(), xBoundaries.end());
    std::sort(zBoundaries.begin(), zBoundaries.end());
    const auto dedupe = [](std::vector<float>& values) {
        values.erase(std::unique(values.begin(), values.end(),
                                 [](float a, float b) { return std::abs(a - b) < 1.0e-7f; }),
                    values.end());
    };
    dedupe(xBoundaries);
    dedupe(zBoundaries);

    const auto insideAnyNotch = [&](float x, float z) {
        if (x > innerEdge && std::abs(z) < channelHalfWidth)
            return true; // +X notch
        if (x < -innerEdge && std::abs(z) < channelHalfWidth)
            return true; // -X notch
        if (z > innerEdge && std::abs(x) < channelHalfWidth)
            return true; // +Z notch
        if (z < -innerEdge && std::abs(x) < channelHalfWidth)
            return true; // -Z notch
        return false;
    };

    for (size_t xi = 0; xi + 1 < xBoundaries.size(); ++xi) {
        for (size_t zi = 0; zi + 1 < zBoundaries.size(); ++zi) {
            const float minX = xBoundaries[xi];
            const float maxX = xBoundaries[xi + 1];
            const float minZ = zBoundaries[zi];
            const float maxZ = zBoundaries[zi + 1];
            const float midX = (minX + maxX) * 0.5f;
            const float midZ = (minZ + maxZ) * 0.5f;

            if (insideAnyNotch(midX, midZ))
                continue;

            AppendBox(outVertices, outIndices, { midX, 0.0f, midZ },
                     { (maxX - minX) * 0.5f, halfLength, (maxZ - minZ) * 0.5f });
        }
    }
}

void GenerateConnectorCornerBracket(float sizeXMeters, float sizeYMeters, float sizeZMeters,
                                    std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const float hx = sizeXMeters * 0.5f;
    const float hy = sizeYMeters * 0.5f;
    const float hz = sizeZMeters * 0.5f;
    const float thickness = juce::jmin(sizeXMeters, juce::jmin(sizeYMeters, sizeZMeters)) * 0.2f;
    const float halfThickness = thickness * 0.5f;

    // Base plate: flat along X/Z at the bottom (-Y) edge of the bounding box.
    AppendBox(outVertices, outIndices, { 0.0f, -hy + halfThickness, 0.0f }, { hx, halfThickness, hz });

    // Upright plate: flat along Y/Z at the -X edge -- together with the base
    // plate this forms a generic right-angle L-bracket.
    AppendBox(outVertices, outIndices, { -hx + halfThickness, 0.0f, 0.0f }, { halfThickness, hy, hz });
}

void GenerateConnectorTNut(float sizeXMeters, float sizeYMeters, float sizeZMeters,
                           std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    AppendBox(outVertices, outIndices, { 0.0f, 0.0f, 0.0f },
             { sizeXMeters * 0.5f, sizeYMeters * 0.5f, sizeZMeters * 0.5f });
}

} // namespace ce
