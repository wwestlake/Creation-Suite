#include "Render/Scene/ProceduralMesh.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

void GenerateDinRailTopHat(float outerWidthMeters, float depthMeters, float lengthMeters,
                          std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    const float halfWidth = outerWidthMeters * 0.5f;
    const float halfLength = lengthMeters * 0.5f;
    const float halfDepth = depthMeters * 0.5f;

    // Base plate: flat mounting web at the back (-Z), full 35mm width.
    const float baseThickness = depthMeters * 0.25f;
    AppendBox(outVertices, outIndices, { 0.0f, 0.0f, -halfDepth + baseThickness * 0.5f },
             { halfWidth, halfLength, baseThickness * 0.5f });

    // Two raised edge ridges, rising to the full front (+Z) face -- stand-in
    // for the curled lip edges. Overlaps the base plate's Z range near the
    // edges; harmless for a solid stand-in mesh, the same "no boolean union
    // needed" approach GenerateConnectorCornerBracket's own two overlapping
    // plates already rely on.
    const float ridgeWidth = outerWidthMeters * 0.18f;
    const float ridgeX = halfWidth - ridgeWidth * 0.5f;
    AppendBox(outVertices, outIndices, { -ridgeX, 0.0f, 0.0f }, { ridgeWidth * 0.5f, halfLength, halfDepth });
    AppendBox(outVertices, outIndices, {  ridgeX, 0.0f, 0.0f }, { ridgeWidth * 0.5f, halfLength, halfDepth });
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

namespace {
// Local 2D point type for this generator's internal math -- juce::Point is
// used only at the public API boundary (matches this file's own convention
// of small local Vec3/BoxVec3 types for generator internals, see AppendBox/
// GenerateCube above).
struct Vec2 {
    float x = 0.0f, y = 0.0f;
};
Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
float Cross2(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
float SignedArea(const std::vector<Vec2>& polygon) {
    float area = 0.0f;
    for (size_t i = 0; i < polygon.size(); ++i) {
        const auto& a = polygon[i];
        const auto& b = polygon[(i + 1) % polygon.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5f;
}
void EnsureWinding(std::vector<Vec2>& polygon, bool wantCcw) {
    const bool isCcw = SignedArea(polygon) > 0.0f;
    if (isCcw != wantCcw)
        std::reverse(polygon.begin(), polygon.end());
}

std::vector<Vec2> CirclePoints(Vec2 center, float diameterMeters, int segments) {
    std::vector<Vec2> points;
    points.reserve(static_cast<size_t>(segments));
    const float radius = diameterMeters * 0.5f;
    const float pi = juce::MathConstants<float>::pi;
    for (int i = 0; i < segments; ++i) {
        const float theta = 2.0f * pi * static_cast<float>(i) / static_cast<float>(segments);
        points.push_back({ center.x + radius * std::cos(theta), center.y + radius * std::sin(theta) });
    }
    return points;
}

// Proper (strict) segment intersection test -- returns false for segments
// that only touch at a shared endpoint or overlap collinearly, which is
// exactly the behavior needed for a hole-bridge visibility check (two
// candidate bridge endpoints are themselves polygon vertices, so any edge
// touching one of them at that exact point must NOT count as an
// obstruction).
bool SegmentsProperlyIntersect(Vec2 a, Vec2 b, Vec2 c, Vec2 d) {
    const float d1 = Cross2(d - c, a - c);
    const float d2 = Cross2(d - c, b - c);
    const float d3 = Cross2(b - a, c - a);
    const float d4 = Cross2(b - a, d - a);
    return ((d1 > 0.0f && d2 < 0.0f) || (d1 < 0.0f && d2 > 0.0f)) &&
           ((d3 > 0.0f && d4 < 0.0f) || (d3 < 0.0f && d4 > 0.0f));
}

bool SegmentIsClearOfLoop(Vec2 a, Vec2 b, const std::vector<Vec2>& loop) {
    for (size_t i = 0; i < loop.size(); ++i) {
        const auto& c = loop[i];
        const auto& d = loop[(i + 1) % loop.size()];
        if (SegmentsProperlyIntersect(a, b, c, d))
            return false;
    }
    return true;
}

// Bridges one hole loop (already CW-wound) into `boundary` (already
// CCW-wound, mutated in place) using the standard ear-clipping-with-holes
// technique: find the hole's rightmost point, find where a rightward ray
// from it first exits through the boundary, and splice the hole's loop into
// the boundary at that crossing edge's far endpoint -- turning "boundary
// with a hole" into one simple polygon with a zero-width slit. Holes are
// bridged one at a time against the boundary as it currently stands
// (including any earlier holes' already-inserted bridges) rather than all
// at once against the pristine original boundary -- simpler to implement
// correctly, and still order-independent in practice since a bridge is a
// zero-width slit that never meaningfully obstructs a later hole's own
// visibility test.
void BridgeHoleIntoBoundary(std::vector<Vec2>& boundary, const std::vector<Vec2>& hole) {
    if (hole.empty())
        return;

    size_t holeConnectIndex = 0;
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].x > hole[holeConnectIndex].x ||
            (hole[i].x == hole[holeConnectIndex].x && hole[i].y > hole[holeConnectIndex].y))
            holeConnectIndex = i;
    }
    const Vec2 holeConnectPoint = hole[holeConnectIndex];

    // Nearest boundary edge crossing a +X ray cast from holeConnectPoint --
    // the standard heuristic starting point for the bridge search.
    int crossingEdgeIndex = -1;
    float nearestCrossingX = std::numeric_limits<float>::max();
    for (size_t i = 0; i < boundary.size(); ++i) {
        const auto& p0 = boundary[i];
        const auto& p1 = boundary[(i + 1) % boundary.size()];
        // Edge must straddle holeConnectPoint's Y to cross a horizontal ray.
        if ((p0.y > holeConnectPoint.y) == (p1.y > holeConnectPoint.y))
            continue;
        const float t = (holeConnectPoint.y - p0.y) / (p1.y - p0.y);
        const float crossX = p0.x + t * (p1.x - p0.x);
        if (crossX >= holeConnectPoint.x && crossX < nearestCrossingX) {
            nearestCrossingX = crossX;
            crossingEdgeIndex = static_cast<int>(i);
        }
    }

    size_t candidateIndex = 0;
    bool candidateFound = false;
    if (crossingEdgeIndex >= 0) {
        const auto i0 = static_cast<size_t>(crossingEdgeIndex);
        const auto i1 = (i0 + 1) % boundary.size();
        candidateIndex = boundary[i0].x >= boundary[i1].x ? i0 : i1;
        if (SegmentIsClearOfLoop(holeConnectPoint, boundary[candidateIndex], boundary) &&
            SegmentIsClearOfLoop(holeConnectPoint, boundary[candidateIndex], hole))
            candidateFound = true;
    }

    // Fallback: brute-force nearest boundary vertex with a clear line of
    // sight -- covers both "the primary heuristic's candidate was blocked"
    // and "no crossing was found at all" (e.g. unusual/degenerate input).
    if (!candidateFound) {
        float bestDistanceSq = std::numeric_limits<float>::max();
        bool any = false;
        for (size_t i = 0; i < boundary.size(); ++i) {
            if (!SegmentIsClearOfLoop(holeConnectPoint, boundary[i], boundary) ||
                !SegmentIsClearOfLoop(holeConnectPoint, boundary[i], hole))
                continue;
            const auto delta = boundary[i] - holeConnectPoint;
            const float distanceSq = delta.x * delta.x + delta.y * delta.y;
            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                candidateIndex = i;
                any = true;
            }
        }
        if (!any)
            candidateIndex = 0; // last resort -- proceeds anyway rather than dropping the hole silently.
    }

    std::vector<Vec2> merged;
    merged.reserve(boundary.size() + hole.size() + 2);
    for (size_t i = 0; i <= candidateIndex; ++i)
        merged.push_back(boundary[i]);
    for (size_t k = 0; k <= hole.size(); ++k)
        merged.push_back(hole[(holeConnectIndex + k) % hole.size()]);
    merged.push_back(boundary[candidateIndex]);
    for (size_t i = candidateIndex + 1; i < boundary.size(); ++i)
        merged.push_back(boundary[i]);

    boundary = std::move(merged);
}

bool PointInOrOnTriangle(Vec2 p, Vec2 a, Vec2 b, Vec2 c) {
    const float d1 = Cross2(b - a, p - a);
    const float d2 = Cross2(c - b, p - b);
    const float d3 = Cross2(a - c, p - c);
    const bool hasNeg = (d1 < -1.0e-9f) || (d2 < -1.0e-9f) || (d3 < -1.0e-9f);
    const bool hasPos = (d1 > 1.0e-9f) || (d2 > 1.0e-9f) || (d3 > 1.0e-9f);
    return !(hasNeg && hasPos);
}

// Standard O(n^2) ear-clipping over a CCW-wound simple polygon (zero-width
// bridge slits from BridgeHoleIntoBoundary included -- operating purely on
// indices rather than positions makes this robust to the duplicate-position
// vertices a bridge introduces). Returns a flat list of triangle index-
// triples into `polygon`. `remaining.size() <= 3` without a clip found is a
// safety valve against pathological/self-intersecting input, not expected
// for hand-placed sketch points.
std::vector<int> TriangulateSimplePolygon(const std::vector<Vec2>& polygon) {
    std::vector<int> remaining(polygon.size());
    for (size_t i = 0; i < polygon.size(); ++i)
        remaining[i] = static_cast<int>(i);

    std::vector<int> triangles;
    while (remaining.size() > 3) {
        bool clipped = false;
        for (size_t i = 0; i < remaining.size(); ++i) {
            const size_t prev = (i + remaining.size() - 1) % remaining.size();
            const size_t next = (i + 1) % remaining.size();
            const int a = remaining[prev];
            const int b = remaining[i];
            const int c = remaining[next];

            if (Cross2(polygon[static_cast<size_t>(b)] - polygon[static_cast<size_t>(a)],
                      polygon[static_cast<size_t>(c)] - polygon[static_cast<size_t>(a)]) <= 1.0e-9f)
                continue; // reflex or degenerate -- not a valid ear tip.

            bool anyInside = false;
            for (size_t k = 0; k < remaining.size(); ++k) {
                if (k == prev || k == i || k == next)
                    continue;
                if (PointInOrOnTriangle(polygon[static_cast<size_t>(remaining[k])], polygon[static_cast<size_t>(a)],
                                        polygon[static_cast<size_t>(b)], polygon[static_cast<size_t>(c)])) {
                    anyInside = true;
                    break;
                }
            }
            if (anyInside)
                continue;

            triangles.push_back(a);
            triangles.push_back(b);
            triangles.push_back(c);
            remaining.erase(remaining.begin() + static_cast<long>(i));
            clipped = true;
            break;
        }
        if (!clipped)
            break;
    }

    if (remaining.size() == 3) {
        triangles.push_back(remaining[0]);
        triangles.push_back(remaining[1]);
        triangles.push_back(remaining[2]);
    }
    return triangles;
}
} // namespace

void GenerateExtrudedPolygonWithHoles(const std::vector<juce::Point<float>>& outerBoundaryUV,
                                      const std::vector<SketchHoleDefinition>& holes,
                                      float thicknessMeters, int circleSegments,
                                      std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    if (outerBoundaryUV.size() < 3)
        return;

    std::vector<Vec2> boundary;
    boundary.reserve(outerBoundaryUV.size());
    for (const auto& p : outerBoundaryUV)
        boundary.push_back({ p.x, p.y });
    EnsureWinding(boundary, true);

    // Original (unbridged) loops, kept separately for side-wall generation
    // below -- the merged/bridged `boundary` (mutated by BridgeHoleIntoBoundary)
    // is only used for the cap triangulation, never for the walls, since its
    // zero-width bridge edges are not real geometry.
    const std::vector<Vec2> originalBoundary = boundary;
    std::vector<std::vector<Vec2>> originalHoles;
    originalHoles.reserve(holes.size());

    for (const auto& hole : holes) {
        auto holeLoop = CirclePoints({ hole.centerUV.x, hole.centerUV.y }, hole.diameterMeters, circleSegments);
        EnsureWinding(holeLoop, false); // holes wind CW, opposite the outer boundary.
        originalHoles.push_back(holeLoop);
        BridgeHoleIntoBoundary(boundary, holeLoop);
    }

    const auto triangleIndices = TriangulateSimplePolygon(boundary);
    const float halfThickness = thicknessMeters * 0.5f;

    // Top cap (+Z) -- triangleIndices are already CCW when viewed from +Z
    // per EnsureWinding's convention above.
    for (size_t t = 0; t + 2 < triangleIndices.size(); t += 3) {
        const auto baseIndex = static_cast<GLuint>(outVertices.size());
        const int idx[3] = { triangleIndices[t], triangleIndices[t + 1], triangleIndices[t + 2] };
        for (int i = 0; i < 3; ++i) {
            Vertex vertex{};
            vertex.position[0] = boundary[static_cast<size_t>(idx[i])].x;
            vertex.position[1] = boundary[static_cast<size_t>(idx[i])].y;
            vertex.position[2] = halfThickness;
            vertex.normal[2] = 1.0f;
            outVertices.push_back(vertex);
        }
        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 1);
        outIndices.push_back(baseIndex + 2);
    }

    // Bottom cap (-Z) -- same triangles, reversed winding for the flipped normal.
    for (size_t t = 0; t + 2 < triangleIndices.size(); t += 3) {
        const auto baseIndex = static_cast<GLuint>(outVertices.size());
        const int idx[3] = { triangleIndices[t], triangleIndices[t + 1], triangleIndices[t + 2] };
        for (int i = 0; i < 3; ++i) {
            Vertex vertex{};
            vertex.position[0] = boundary[static_cast<size_t>(idx[i])].x;
            vertex.position[1] = boundary[static_cast<size_t>(idx[i])].y;
            vertex.position[2] = -halfThickness;
            vertex.normal[2] = -1.0f;
            outVertices.push_back(vertex);
        }
        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 1);
    }

    // Side walls -- every ORIGINAL boundary/hole edge (never the merged
    // loop's zero-width bridges). 2D outward normal for a CCW edge (p0->p1)
    // is normalize(dy,-dx); the same formula also gives the correct
    // toward-the-hole-center normal for a CW hole edge (both are "away from
    // the solid material," which is what makes one formula work for both --
    // see the plan's Part O derivation).
    const auto appendWalls = [&](const std::vector<Vec2>& loop) {
        for (size_t i = 0; i < loop.size(); ++i) {
            const auto& p0 = loop[i];
            const auto& p1 = loop[(i + 1) % loop.size()];
            const Vec2 edge = p1 - p0;
            const float length = std::sqrt(edge.x * edge.x + edge.y * edge.y);
            if (length < 1.0e-9f)
                continue;
            const Vec2 normal2D { edge.y / length, -edge.x / length };

            const auto baseIndex = static_cast<GLuint>(outVertices.size());
            const Vec2 quadUV[4] = { p0, p1, p1, p0 };
            const float quadZ[4] = { -halfThickness, -halfThickness, halfThickness, halfThickness };
            for (int i2 = 0; i2 < 4; ++i2) {
                Vertex vertex{};
                vertex.position[0] = quadUV[i2].x;
                vertex.position[1] = quadUV[i2].y;
                vertex.position[2] = quadZ[i2];
                vertex.normal[0] = normal2D.x;
                vertex.normal[1] = normal2D.y;
                outVertices.push_back(vertex);
            }
            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 1);
            outIndices.push_back(baseIndex + 2);
            outIndices.push_back(baseIndex + 0);
            outIndices.push_back(baseIndex + 2);
            outIndices.push_back(baseIndex + 3);
        }
    };

    appendWalls(originalBoundary);
    for (const auto& hole : originalHoles)
        appendWalls(hole);
}

void BuildFlatShadedMeshFromCage(const std::vector<juce::Vector3D<float>>& corners,
                                 std::vector<Vertex>& outVertices, std::vector<GLuint>& outIndices) {
    outVertices.clear();
    outIndices.clear();

    if (corners.size() != 8)
        return;

    // Each row is one face's 4 corner indices, ordered so cross((b-a),(c-a))
    // already points outward for the standard (undragged) box -- verified
    // against the fixed bit-order corner scheme documented in the header.
    static const int faces[6][4] = {
        { 0, 4, 6, 2 }, // -X
        { 1, 3, 7, 5 }, // +X
        { 0, 1, 5, 4 }, // -Y
        { 2, 6, 7, 3 }, // +Y
        { 0, 2, 3, 1 }, // -Z
        { 4, 5, 7, 6 }, // +Z
    };

    for (const auto& face : faces) {
        const auto& a = corners[static_cast<size_t>(face[0])];
        const auto& b = corners[static_cast<size_t>(face[1])];
        const auto& c = corners[static_cast<size_t>(face[2])];
        const auto& d = corners[static_cast<size_t>(face[3])];

        const auto normal = ((b - a) ^ (c - a)).normalised();
        const juce::Vector3D<float> quad[4] = { a, b, c, d };
        const float uvs[4][2] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };

        const auto baseIndex = static_cast<GLuint>(outVertices.size());
        for (int i = 0; i < 4; ++i) {
            Vertex vertex{};
            vertex.position[0] = quad[i].x;
            vertex.position[1] = quad[i].y;
            vertex.position[2] = quad[i].z;
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;
            vertex.uv[0] = uvs[i][0];
            vertex.uv[1] = uvs[i][1];
            outVertices.push_back(vertex);
        }

        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 1);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 0);
        outIndices.push_back(baseIndex + 2);
        outIndices.push_back(baseIndex + 3);
    }
}

} // namespace ce
