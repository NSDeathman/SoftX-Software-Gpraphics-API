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

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <algorithm>

#include "math_float3.h"
#include "math_ray.h"
#include "AfterMathInternal.h"

AFTERMATH_BEGIN

// Forward declaration
class AABB;

// ============================================================================
// Axis-Aligned Bounding Box Class
// ============================================================================

/**
 * @class AABB
 * @brief Axis-aligned bounding box defined by min and max corners
 *
 * Represents an axis-aligned box using two float3 points:
 *   min = lower corner, max = upper corner.
 *
 * The box is considered "valid" when min <= max component-wise.
 * Default constructor creates an invalid (empty) box that can be expanded.
 *
 * @note All methods are noexcept and follow the library's conventions.
 */
class AABB
{
public:
    float3 min; ///< Minimum corner of the box
    float3 max; ///< Maximum corner of the box

    // ============================================================================
    // Constructors
    // ============================================================================

    /// Creates an empty box (min = +∞, max = -∞)
    AABB() noexcept
        : min(INFINITY, INFINITY, INFINITY)
        , max(-INFINITY, -INFINITY, -INFINITY) {}

    AABB(const float3& min, const float3& max) noexcept
        : min(min), max(max) {}

    AABB(const AABB&) noexcept = default;

    // ============================================================================
    // Assignment Operators
    // ============================================================================

    AABB& operator=(const AABB&) noexcept = default;

    // ============================================================================
    // Static Constructors
    // ============================================================================

    /// Create AABB from center and half-extents
    static AABB from_center_extents(const float3& center, const float3& extents) noexcept {
        return AABB(center - extents, center + extents);
    }

    /// Create AABB from an array of points
    static AABB from_points(const float3* points, size_t count) noexcept {
        AABB box;
        for (size_t i = 0; i < count; ++i) {
            box.expand(points[i]);
        }
        return box;
    }

    // ============================================================================
    // Basic Properties
    // ============================================================================

    float3 center() const noexcept { return (min + max) * 0.5f; }
    float3 extents() const noexcept { return (max - min) * 0.5f; }
    float3 size() const noexcept { return max - min; }

    bool is_valid() const noexcept {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }

    // ============================================================================
    // Expansion / Modification
    // ============================================================================

    /// Expand the box to include a point
    void expand(const float3& point) noexcept {
        min = AfterMath::min(min, point);
        max = AfterMath::max(max, point);
    }

    /// Expand the box to include another AABB
    void expand(const AABB& other) noexcept {
        min = AfterMath::min(min, other.min);
        max = AfterMath::max(max, other.max);
    }

    // ============================================================================
    // Containment Tests
    // ============================================================================

    bool contains(const float3& point) const noexcept {
        return  point.x >= min.x && point.x <= max.x &&
                point.y >= min.y && point.y <= max.y &&
                point.z >= min.z && point.z <= max.z;
    }

    bool contains(const AABB& other) const noexcept {
        return contains(other.min) && contains(other.max);
    }

    bool intersects(const AABB& other) const noexcept {
        return  (min.x <= other.max.x && max.x >= other.min.x) &&
                (min.y <= other.max.y && max.y >= other.min.y) &&
                (min.z <= other.max.z && max.z >= other.min.z);
    }

    // ============================================================================
    // Ray Intersection
    // ============================================================================

    /**
     * @brief Intersect with a ray (slab method).
     * @param r Ray to test
     * @return Closest t >= 0 if hit, -1 otherwise.
     */
    float intersect(const ray& r) const noexcept {
        return intersect_ray_aabb(r, min, max);
    }

    /// Alias for intersect()
    float intersect_ray(const ray& r) const noexcept {
        return intersect(r);
    }

    // ============================================================================
    // Utility Methods
    // ============================================================================

    std::string to_string() const {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "AABB(min=(%.3f, %.3f, %.3f), max=(%.3f, %.3f, %.3f))", min.x, min.y, min.z, max.x, max.y, max.z);
        return std::string(buffer);
    }
};

// ============================================================================
// Global AABB Functions
// ============================================================================

inline AABB aabb_from_center_extents(const float3& center, const float3& extents) noexcept {
    return AABB::from_center_extents(center, extents);
}

inline AABB aabb_from_points(const float3* points, size_t count) noexcept {
    return AABB::from_points(points, count);
}

inline AABB aabb_union(const AABB& a, const AABB& b) noexcept {
    AABB result(a);
    result.expand(b);
    return result;
}

inline bool aabb_intersects(const AABB& a, const AABB& b) noexcept {
    return a.intersects(b);
}

inline float aabb_intersect_ray(const AABB& box, const ray& r) noexcept {
    return box.intersect(r);
}

// ============================================================================
// Comparison Operators
// ============================================================================

inline bool approximately(const AABB& a, const AABB& b, float epsilon = EPSILON) noexcept {
    return approximately(a.min, b.min, epsilon) && approximately(a.max, b.max, epsilon);
}

inline bool operator==(const AABB& a, const AABB& b) noexcept {
    return approximately(a, b);
}

inline bool operator!=(const AABB& a, const AABB& b) noexcept {
    return !approximately(a, b);
}

AFTERMATH_END
