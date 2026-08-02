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

#include <string>
#include <cstdio>
#include <algorithm>
#include <cmath>

#include "AfterMathInternal.h"
#include "math_float2.h"

AFTERMATH_BEGIN

// ============================================================================
// 2D Axis-Aligned Rectangle (float2)
// ============================================================================

/**
 * @struct Rect
 * @brief 2‑dimensional axis‑aligned rectangle (position + size) using float2.
 *
 * Represents a rectangle by its top‑left corner (position) and dimensions (size).
 * All methods are constexpr/noexcept and work with float2 for seamless integration
 * with the rest of AfterMath.
 *
 * @note For intersection/union semantics, the rectangle is considered closed
 *       on the left/top edges and open on the right/bottom edges,
 *       similarly to many graphics APIs.
 */
struct Rect
{
    float2 position;  ///< Top‑left corner
    float2 size;      ///< Width and height (must be ≥ 0)

    // ========================================================================
    // Constructors
    // ========================================================================

    constexpr Rect() noexcept
        : position(0.0f), size(0.0f) {}

    constexpr Rect(const float2& top_left, const float2& dimensions) noexcept
        : position(top_left), size(dimensions) {}

    /**
     * @brief Construct from left, top, width, height.
     */
    constexpr Rect(float left, float top, float width, float height) noexcept
        : position(left, top), size(width, height) {}

    /**
     * @brief Construct from min and max corners.
     *        The resulting rectangle always has non‑negative size.
     */
    static constexpr Rect from_min_max(const float2& min, const float2& max) noexcept
    {
        return Rect(min, max - min);
    }

    /**
     * @brief Construct from center and half‑extents.
     */
    static constexpr Rect from_center_extents(const float2& center, const float2& half_extents) noexcept
    {
        return Rect(center - half_extents, half_extents * 2.0f);
    }

    // ========================================================================
    // Basic Accessors
    // ========================================================================

    constexpr float left()   const noexcept { return position.x; }
    constexpr float top()    const noexcept { return position.y; }
    constexpr float width()  const noexcept { return size.x; }
    constexpr float height() const noexcept { return size.y; }

    inline float right()  const noexcept { return left() + width(); }
    inline float bottom() const noexcept { return top() + height(); }

    constexpr float2 min() const noexcept { return position; }
    inline float2 max() const noexcept { return position + size; }

    inline float2 center() const noexcept
    {
        return position + size * 0.5f;
    }

    inline float2 extent() const noexcept
    {
        return size * 0.5f;
    }

    // ========================================================================
    // Derived Properties
    // ========================================================================

    constexpr float area() const noexcept
    {
        return size.x * size.y;
    }

    /**
     * @brief Aspect ratio (width / height). Returns 0 if height is zero.
     */
    inline float aspect_ratio() const noexcept
    {
        return (size.y != 0.0f) ? (size.x / size.y) : 0.0f;
    }

    inline bool is_valid() const noexcept
    {
        return size.x >= 0.0f && size.y >= 0.0f &&
               std::isfinite(position.x) && std::isfinite(position.y) &&
               std::isfinite(size.x) && std::isfinite(size.y);
    }

    // ========================================================================
    // Containment & Overlap Tests
    // ========================================================================

    /**
     * @brief True if point is inside (including left/top edges, excluding right/bottom).
     */
    inline bool contains(const float2& point) const noexcept
    {
        return point.x >= left() && point.x < right() &&
               point.y >= top()  && point.y < bottom();
    }

    /**
     * @brief True if another rectangle is completely inside this one.
     */
    inline bool contains(const Rect& other) const noexcept
    {
        return other.left()   >= left()   && other.right()  <= right() &&
               other.top()    >= top()    && other.bottom() <= bottom();
    }

    /**
     * @brief True if the two rectangles intersect (share at least one point).
     */
    inline bool overlaps(const Rect& other) const noexcept
    {
        return left()   < other.right()  && right()  > other.left() &&
               top()    < other.bottom() && bottom() > other.top();
    }

    // ========================================================================
    // Mutating Operations (return new Rect)
    // ========================================================================

    /**
     * @brief Intersection of two rectangles. Returns empty Rect if no overlap.
     */
    inline Rect intersected(const Rect& other) const noexcept
    {
        float l = std::max(left(),   other.left());
        float t = std::max(top(),    other.top());
        float r = std::min(right(),  other.right());
        float b = std::min(bottom(), other.bottom());

        if (l < r && t < b)
            return Rect(l, t, r - l, b - t);
        else
            return Rect(); // empty
    }

    /**
     * @brief Union of two rectangles (bounding box containing both).
     */
    constexpr Rect united(const Rect& other) const noexcept
    {
        if (size.x == 0.0f && size.y == 0.0f)
            return other;
        if (other.size.x == 0.0f && other.size.y == 0.0f)
            return *this;

        float l = std::min(left(), other.left());
        float t = std::min(top(), other.top());
        float r = std::max(right(), other.right());
        float b = std::max(bottom(), other.bottom());
        return Rect(l, t, r - l, b - t);
    }

    /**
     * @brief Expand the rectangle by adding margins on all sides.
     */
    inline Rect expanded(const float2& amounts) const noexcept
    {
        return Rect(position - amounts, size + amounts * 2.0f);
    }

    /**
     * @brief Expand the rectangle by a uniform amount.
     */
    inline Rect expanded(float amount) const noexcept
    {
        return expanded(float2(amount));
    }

    /**
     * @brief Translate the rectangle by a vector.
     */
    inline Rect translated(const float2& offset) const noexcept
    {
        return Rect(position + offset, size);
    }

    // ========================================================================
    // Comparison (floating‑point tolerant via float2::operator==)
    // ========================================================================

    friend bool operator==(const Rect& a, const Rect& b) noexcept
    {
        return a.position == b.position && a.size == b.size;
    }

    friend bool operator!=(const Rect& a, const Rect& b) noexcept
    {
        return !(a == b);
    }

    // ========================================================================
    // String Output
    // ========================================================================

    std::string to_string() const
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf),
            "Rect(L:%.3f, T:%.3f, R:%.3f, B:%.3f | W:%.3f, H:%.3f)",
            left(), top(), right(), bottom(), width(), height());
        return std::string(buf);
    }
};

// ============================================================================
// HLSL‑like Global Functions for Rect
// ============================================================================

inline Rect rect_from_min_max(const float2& min, const float2& max) noexcept
{
    return Rect::from_min_max(min, max);
}

inline Rect rect_from_center_extents(const float2& center, const float2& half_extents) noexcept
{
    return Rect::from_center_extents(center, half_extents);
}

inline Rect rect_intersection(const Rect& a, const Rect& b) noexcept
{
    return a.intersected(b);
}

inline Rect rect_union(const Rect& a, const Rect& b) noexcept
{
    return a.united(b);
}

inline bool rect_contains(const Rect& r, const float2& point) noexcept
{
    return r.contains(point);
}

inline bool rect_contains(const Rect& a, const Rect& b) noexcept
{
    return a.contains(b);
}

inline bool rect_overlaps(const Rect& a, const Rect& b) noexcept
{
    return a.overlaps(b);
}

inline Rect rect_expand(const Rect& r, float amount) noexcept
{
    return r.expanded(amount);
}

inline Rect rect_expand(const Rect& r, const float2& amounts) noexcept
{
    return r.expanded(amounts);
}

inline Rect rect_translate(const Rect& r, const float2& offset) noexcept
{
    return r.translated(offset);
}

// ============================================================================
// Useful Constants
// ============================================================================

AFTERMATH_INLINE_VAR const Rect Rect_Zero(0.0f, 0.0f, 0.0f, 0.0f);
AFTERMATH_INLINE_VAR const Rect Rect_Unit(0.0f, 0.0f, 1.0f, 1.0f);

AFTERMATH_END
