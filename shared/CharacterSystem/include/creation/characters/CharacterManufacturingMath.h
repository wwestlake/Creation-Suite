#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace creation::characters::math
{

// Data-only character-manufacturing math. This layer has no dependency on an
// app, renderer, VFS implementation, physics library, or DCC tool.
constexpr float kEpsilon = 1.0e-6f;

struct Vec3 final
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

[[nodiscard]] constexpr Vec3 Add(Vec3 a, Vec3 b) noexcept { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
[[nodiscard]] constexpr Vec3 Subtract(Vec3 a, Vec3 b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
[[nodiscard]] constexpr Vec3 Scale(Vec3 value, float scalar) noexcept { return { value.x * scalar, value.y * scalar, value.z * scalar }; }
[[nodiscard]] constexpr float Dot(Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
[[nodiscard]] constexpr Vec3 Cross(Vec3 a, Vec3 b) noexcept
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
[[nodiscard]] inline float Length(Vec3 value) noexcept { return std::sqrt(Dot(value, value)); }
[[nodiscard]] inline Vec3 Normalize(Vec3 value, Vec3 fallback = {}) noexcept
{
    const float length = Length(value);
    return length > kEpsilon ? Scale(value, 1.0f / length) : fallback;
}
[[nodiscard]] constexpr float Clamp(float value, float minimum, float maximum) noexcept
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}
[[nodiscard]] constexpr float RemapClamped(float value, float sourceMinimum, float sourceMaximum,
                                             float destinationMinimum, float destinationMaximum) noexcept
{
    const float span = sourceMaximum - sourceMinimum;
    if (span <= kEpsilon) return destinationMinimum;
    const float t = Clamp((value - sourceMinimum) / span, 0.0f, 1.0f);
    return destinationMinimum + (destinationMaximum - destinationMinimum) * t;
}

struct Bounds final
{
    Vec3 minimum{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
    Vec3 maximum{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

    [[nodiscard]] bool IsValid() const noexcept { return minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z; }
    [[nodiscard]] Vec3 Extents() const noexcept { return IsValid() ? Subtract(maximum, minimum) : Vec3{}; }
};

inline void Expand(Bounds& bounds, Vec3 point) noexcept
{
    bounds.minimum.x = std::min(bounds.minimum.x, point.x); bounds.minimum.y = std::min(bounds.minimum.y, point.y); bounds.minimum.z = std::min(bounds.minimum.z, point.z);
    bounds.maximum.x = std::max(bounds.maximum.x, point.x); bounds.maximum.y = std::max(bounds.maximum.y, point.y); bounds.maximum.z = std::max(bounds.maximum.z, point.z);
}

[[nodiscard]] inline Bounds MeasureBounds(std::span<const Vec3> vertices) noexcept
{
    Bounds bounds;
    for (const auto vertex : vertices) Expand(bounds, vertex);
    return bounds;
}

struct CharacterMeasurements final
{
    Bounds bounds;
    float heightMeters = 0.0f;
    float widthMeters = 0.0f;
    float depthMeters = 0.0f;
    float groundOffsetMeters = 0.0f;
};

// The importer converts source coordinates into the suite's Y-up convention
// before this layer runs. Math code never guesses a source tool's axes.
[[nodiscard]] inline CharacterMeasurements MeasureCharacter(std::span<const Vec3> vertices) noexcept
{
    CharacterMeasurements result; result.bounds = MeasureBounds(vertices);
    if (!result.bounds.IsValid()) return result;
    const auto extents = result.bounds.Extents();
    result.heightMeters = extents.y; result.widthMeters = extents.x; result.depthMeters = extents.z;
    result.groundOffsetMeters = result.bounds.minimum.y;
    return result;
}

struct CapsuleDimensions final { float radiusMeters = 0.0f; float halfCylinderHeightMeters = 0.0f; };

// Policy comes from the recipe or creature class; there is no hidden human
// proportion assumption in Engine code.
[[nodiscard]] inline CapsuleDimensions SuggestCapsule(const CharacterMeasurements& measurements,
                                                        float horizontalRadiusFraction,
                                                        float minimumRadiusMeters,
                                                        float maximumRadiusMeters) noexcept
{
    const float radius = Clamp(std::max(measurements.widthMeters, measurements.depthMeters) * horizontalRadiusFraction,
                               minimumRadiusMeters, maximumRadiusMeters);
    return { radius, std::max(0.0f, measurements.heightMeters * 0.5f - radius) };
}

struct SparseVertexDelta final { std::uint32_t vertexIndex = 0; Vec3 delta; };
struct WeightedShapeTarget final { std::span<const SparseVertexDelta> deltas; float weight = 0.0f; };

// Caller owns the output buffer. A bad vertex index reports failure instead
// of silently corrupting a different topology.
[[nodiscard]] inline bool EvaluateShape(std::span<const Vec3> baseVertices,
                                        std::span<const WeightedShapeTarget> targets,
                                        std::span<Vec3> outputVertices) noexcept
{
    if (outputVertices.size() != baseVertices.size()) return false;
    std::copy(baseVertices.begin(), baseVertices.end(), outputVertices.begin());
    for (const auto& target : targets)
    {
        if (std::abs(target.weight) <= kEpsilon) continue;
        for (const auto& change : target.deltas)
        {
            if (change.vertexIndex >= outputVertices.size()) return false;
            outputVertices[change.vertexIndex] = Add(outputVertices[change.vertexIndex], Scale(change.delta, target.weight));
        }
    }
    return true;
}

struct Triangle final { std::array<std::uint32_t, 3> vertexIndices{}; };
[[nodiscard]] inline bool IsValidTriangle(const Triangle& triangle, std::size_t vertexCount) noexcept
{
    return triangle.vertexIndices[0] < vertexCount && triangle.vertexIndices[1] < vertexCount && triangle.vertexIndices[2] < vertexCount;
}
[[nodiscard]] inline Vec3 TriangleNormal(const Triangle& triangle, std::span<const Vec3> vertices) noexcept
{
    if (!IsValidTriangle(triangle, vertices.size())) return {};
    const auto a = vertices[triangle.vertexIndices[0]], b = vertices[triangle.vertexIndices[1]], c = vertices[triangle.vertexIndices[2]];
    return Normalize(Cross(Subtract(b, a), Subtract(c, a)));
}
[[nodiscard]] inline Vec3 EvaluateBarycentric(const Triangle& triangle, std::span<const Vec3> vertices,
                                               std::array<float, 3> weights) noexcept
{
    if (!IsValidTriangle(triangle, vertices.size())) return {};
    return Add(Add(Scale(vertices[triangle.vertexIndices[0]], weights[0]), Scale(vertices[triangle.vertexIndices[1]], weights[1])),
               Scale(vertices[triangle.vertexIndices[2]], weights[2]));
}

struct SurfaceBinding final
{
    std::uint32_t triangleIndex = 0;
    std::array<float, 3> barycentricWeights{ 1.0f, 0.0f, 0.0f };
    float normalOffsetMeters = 0.0f;
};

// Garment build data binds once to compatible body topology. Evaluation then
// follows body morphs without an expensive per-frame nearest-triangle search.
[[nodiscard]] inline bool EvaluateSurfaceBinding(const SurfaceBinding& binding,
                                                  std::span<const Triangle> triangles,
                                                  std::span<const Vec3> bodyVertices,
                                                  Vec3& position) noexcept
{
    if (binding.triangleIndex >= triangles.size()) return false;
    const auto& triangle = triangles[binding.triangleIndex];
    if (!IsValidTriangle(triangle, bodyVertices.size())) return false;
    const float sum = binding.barycentricWeights[0] + binding.barycentricWeights[1] + binding.barycentricWeights[2];
    if (std::abs(sum - 1.0f) > 1.0e-3f) return false;
    position = Add(EvaluateBarycentric(triangle, bodyVertices, binding.barycentricWeights),
                   Scale(TriangleNormal(triangle, bodyVertices), binding.normalOffsetMeters));
    return true;
}

struct BoneWeight final { std::uint16_t jointIndex = 0; float weight = 0.0f; };
struct WeightValidation final { bool valid = false; bool normalized = false; float originalSum = 0.0f; };

// Normalizes a chosen influence set in place. Callers choose/prune the most
// significant influences before passing them here.
[[nodiscard]] inline WeightValidation NormalizeBoneWeights(std::span<BoneWeight> influences,
                                                            std::size_t maximumInfluences = 4) noexcept
{
    WeightValidation result;
    if (influences.empty() || influences.size() > maximumInfluences) return result;
    for (const auto& influence : influences)
    {
        if (!std::isfinite(influence.weight) || influence.weight < 0.0f) return result;
        result.originalSum += influence.weight;
    }
    if (result.originalSum <= kEpsilon) return result;
    for (auto& influence : influences) influence.weight /= result.originalSum;
    result.valid = true; result.normalized = std::abs(result.originalSum - 1.0f) > 1.0e-4f;
    return result;
}

[[nodiscard]] inline bool NormalizeBlendWeights(std::span<float> weights) noexcept
{
    float sum = 0.0f;
    for (auto& weight : weights)
    {
        if (!std::isfinite(weight)) return false;
        weight = std::max(0.0f, weight); sum += weight;
    }
    if (sum <= kEpsilon) return false;
    for (auto& weight : weights) weight /= sum;
    return true;
}

} // namespace creation::characters::math
