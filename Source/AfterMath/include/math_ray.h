/*
 * AfterMath — high‑performance C++ math library (HLSL‑style, SSE‑accelerated)
 *
 * Project:   Presence AfterMath
 * Copyright: 2026 Presence Collaboratory
 * Authors:   NSDeathman (Architecture & Core)
 *            DeepSeek (Mathematics & HLSL Integration)
 *            Gemini 3 (Optimization & Fast Math)
 *            Nikolay Partas (Half precision data type prototype)
 * License:   MIT License with Attribution — see LICENSE.md for details.
 *
 * https://github.com/Presence-Collaboratory/AfterMath-CPP-Open-Math-Library
 */
#pragma once

#include <cmath>
#include <cstdio>
#include <algorithm>

#include "math_float3.h"
#include "math_float4.h"
#include "math_float4x4.h"
#include "AfterMathInternal.h"

AFTERMATH_BEGIN

// Forward declarations
class ray;

// ============================================================================
// Ray Class
// ============================================================================

/**
 * @class ray
 * @brief Infinite ray with origin and direction
 *
 * Represents a ray as a point (origin) and a direction vector.
 * Direction should generally be normalized for t to correspond to distance.
 *
 * Uses the same conventions as the rest of the library:
 *  - float3 for origin and direction
 *  - row-vector transformations (transform_point / transform_vector)
 */
class ray
{
public:
    float3 origin;    ///< Ray origin point
    float3 direction; ///< Ray direction (should be normalized for distance = t)

    // ============================================================================
    // Constructors
    // ============================================================================

    ray() noexcept
        : origin(float3::zero())
        , direction(float3::forward()) {}

    ray(const float3& origin, const float3& direction) noexcept
        : origin(origin)
        , direction(direction) {}

    ray(const ray&) noexcept = default;

    // ============================================================================
    // Assignment Operators
    // ============================================================================

    ray& operator=(const ray&) noexcept = default;

    // ============================================================================
    // Methods
    // ============================================================================

    /// Get a point on the ray at parameter t
    float3 point_at(float t) const noexcept {
        return origin + direction * t;
    }

    /// Alias for point_at
    float3 at(float t) const noexcept {
        return point_at(t);
    }

    /// Return a copy with normalized direction
    ray normalized() const noexcept {
        return ray(origin, normalize(direction));
    }

    /// Transform the ray by a matrix (origin as point, direction as vector)
    ray transformed(const float4x4& mat) const noexcept {
        return ray(transform_point(mat, origin), transform_vector(mat, direction));
    }

    /// Transform in-place
    void transform(const float4x4& mat) noexcept {
        origin = transform_point(mat, origin);
        direction = transform_vector(mat, direction);
    }

    std::string to_string() const {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "Ray(origin=(%.3f, %.3f, %.3f), direction=(%.3f, %.3f, %.3f))",
            origin.x, origin.y, origin.z,
            direction.x, direction.y, direction.z);
        return std::string(buffer);
    }
};

// ============================================================================
// Global Functions
// ============================================================================

/// Create a ray from two points (direction = normalized(end - start))
inline ray ray_from_points(const float3& start, const float3& end) noexcept {
    return ray(start, normalize(end - start));
}

inline float3 point_on_ray(const ray& r, float t) noexcept {
    return r.point_at(t);
}

// ============================================================================
// Intersection Tests
// ============================================================================

/**
 * @brief Intersect ray with plane ax + by + cz + d = 0.
 * @param r Ray
 * @param plane float4(a, b, c, d)
 * @return t >= 0 if intersection in front of origin, -1 if parallel or behind.
 */
inline float intersect_ray_plane(const ray& r, const float4& plane) noexcept {
    float3 normal(plane.x, plane.y, plane.z);
    float denom = dot(normal, r.direction);

    if (std::abs(denom) < EPSILON)
        return -1.0f;

    float t = -(dot(normal, r.origin) + plane.w) / denom;
    return (t >= 0.0f) ? t : -1.0f;
}

/**
 * @brief Intersect ray with sphere.
 * @param r Ray
 * @param center Sphere center
 * @param radius Sphere radius
 * @return Closest t >= 0, or -1 if no intersection.
 */
inline float intersect_ray_sphere(const ray& r, const float3& center, float radius) noexcept {
    float3 oc = r.origin - center;
    float b = dot(oc, r.direction);
    float c = dot(oc, oc) - radius * radius;
    float disc = b * b - c;

    if (disc < 0.0f)
        return -1.0f;

    float sqrt_disc = std::sqrt(disc);
    float t1 = -b - sqrt_disc;
    float t2 = -b + sqrt_disc;

    if (t1 >= 0.0f) return t1;
    if (t2 >= 0.0f) return t2;
    return -1.0f;
}

/**
 * @brief Intersect ray with an axis-aligned bounding box (slab method).
 * @param r Ray
 * @param min_bound AABB minimum corner
 * @param max_bound AABB maximum corner
 * @return t >= 0 if hit, -1 otherwise (t may be 0 if origin inside the box).
 */
inline float intersect_ray_aabb(const ray& r, const float3& min_bound, const float3& max_bound) noexcept {
    float t_min = 0.0f;
    float t_max = INFINITY;

    for (int i = 0; i < 3; ++i) {
        float o = r.origin[i];
        float d = r.direction[i];
        float mn = min_bound[i];
        float mx = max_bound[i];

        if (std::abs(d) < EPSILON) {
            // Ray parallel to slab; no hit if origin outside slab
            if (o < mn || o > mx)
                return -1.0f;
            continue;
        }

        float inv_d = 1.0f / d;
        float t1 = (mn - o) * inv_d;
        float t2 = (mx - o) * inv_d;
        if (t1 > t2) std::swap(t1, t2);

        t_min = std::max(t_min, t1);
        t_max = std::min(t_max, t2);

        if (t_min > t_max)
            return -1.0f;
    }

    return t_min;
}

// ============================================================================
// Comparison Operators
// ============================================================================

inline bool approximately(const ray& a, const ray& b, float epsilon = EPSILON) noexcept {
    return approximately(a.origin, b.origin, epsilon) &&
        approximately(a.direction, b.direction, epsilon);
}

inline bool operator==(const ray& a, const ray& b) noexcept {
    return approximately(a, b);
}

inline bool operator!=(const ray& a, const ray& b) noexcept {
    return !approximately(a, b);
}

AFTERMATH_END
