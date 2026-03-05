// math_float4x4.h
// Description: 4x4 matrix class with HLSL-like syntax and SSE optimization
// Author: NSDeathman, DeepSeek

#pragma once

#include <cmath>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cassert>
#include <xmmintrin.h>
#include <pmmintrin.h>
#include <smmintrin.h>

#include "math_float2.h"
#include "math_float3.h"
#include "math_float4.h"
#include "math_float3x3.h"
#include "math_functions.h"
#include "AfterMathInternal.h"

AFTERMATH_BEGIN

// Forward declarations
class quaternion;

inline float4x4 operator*(const float4x4& a, const float4x4& b) noexcept;

// ============================================================================
// 4x4 Matrix Class
// ============================================================================

/**
    * @class float4x4
    * @brief 4x4 matrix with HLSL-like syntax
    *
    * Represents a 4x4 matrix for 3D transformations (translation, rotation, scaling),
    * perspective projections, and view transformations. Uses row-major storage for
    * compatibility with HLSL conventions.
    *
    * @note Size is exactly 64 bytes (4 float4 rows with 16-byte alignment each)
    */
class float4x4
{
public:
    // Data members (public for direct access, aligned for SSE)
    alignas(16) float4 row0; ///< First row of the matrix
    alignas(16) float4 row1; ///< Second row of the matrix
    alignas(16) float4 row2; ///< Third row of the matrix
    alignas(16) float4 row3; ///< Fourth row of the matrix

    // ============================================================================
    // Constructors
    // ============================================================================

    float4x4() noexcept
        : row0(1.0f, 0.0f, 0.0f, 0.0f)
        , row1(0.0f, 1.0f, 0.0f, 0.0f)
        , row2(0.0f, 0.0f, 1.0f, 0.0f)
        , row3(0.0f, 0.0f, 0.0f, 1.0f) {}

    float4x4(const float4& r0, const float4& r1, const float4& r2, const float4& r3) noexcept
        : row0(r0), row1(r1), row2(r2), row3(r3) {}

    float4x4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33) noexcept
        : row0(m00, m01, m02, m03)
        , row1(m10, m11, m12, m13)
        , row2(m20, m21, m22, m23)
        , row3(m30, m31, m32, m33) {}

    explicit float4x4(const float* data) noexcept
        : row0(data[0], data[1], data[2], data[3])
        , row1(data[4], data[5], data[6], data[7])
        , row2(data[8], data[9], data[10], data[11])
        , row3(data[12], data[13], data[14], data[15]) {}

    explicit float4x4(float scalar) noexcept
        : row0(scalar, 0.0f, 0.0f, 0.0f)
        , row1(0.0f, scalar, 0.0f, 0.0f)
        , row2(0.0f, 0.0f, scalar, 0.0f)
        , row3(0.0f, 0.0f, 0.0f, scalar) {}

    explicit float4x4(const float4& diagonal) noexcept
        : row0(diagonal.x, 0.0f, 0.0f, 0.0f)
        , row1(0.0f, diagonal.y, 0.0f, 0.0f)
        , row2(0.0f, 0.0f, diagonal.z, 0.0f)
        , row3(0.0f, 0.0f, 0.0f, diagonal.w) {}

    float4x4(const float4x4&) noexcept = default;

    // ============================================================================
    // Assignment Operators
    // ============================================================================

    float4x4& operator=(const float4x4&) noexcept = default;

    float4x4& operator=(float scalar) noexcept {
        row0 = float4(scalar, 0.0f, 0.0f, 0.0f);
        row1 = float4(0.0f, scalar, 0.0f, 0.0f);
        row2 = float4(0.0f, 0.0f, scalar, 0.0f);
        row3 = float4(0.0f, 0.0f, 0.0f, scalar);
        return *this;
    }

    // ============================================================================
    // Compound Assignment Operators
    // ============================================================================

    float4x4& operator+=(const float4x4& rhs) noexcept {
        row0 += rhs.row0;
        row1 += rhs.row1;
        row2 += rhs.row2;
        row3 += rhs.row3;
        return *this;
    }

    float4x4& operator-=(const float4x4& rhs) noexcept {
        row0 -= rhs.row0;
        row1 -= rhs.row1;
        row2 -= rhs.row2;
        row3 -= rhs.row3;
        return *this;
    }

    float4x4& operator*=(float scalar) noexcept {
        row0 *= scalar;
        row1 *= scalar;
        row2 *= scalar;
        row3 *= scalar;
        return *this;
    }

    float4x4& operator/=(float scalar) noexcept {
        row0 /= scalar;
        row1 /= scalar;
        row2 /= scalar;
        row3 /= scalar;
        return *this;
    }

    float4x4& operator*=(const float4x4& rhs) noexcept {
        *this = *this * rhs;
        return *this;
    }

    // ============================================================================
    // Access Operators
    // ============================================================================

    float4& operator[](int rowIndex) noexcept {
        return (&row0)[rowIndex];
    }

    const float4& operator[](int rowIndex) const noexcept {
        return (&row0)[rowIndex];
    }

    float& operator()(int row, int col) noexcept {
        return (&row0)[row][col];
    }

    const float& operator()(int row, int col) const noexcept {
        return (&row0)[row][col];
    }

    // ============================================================================
    // Column Access Methods
    // ============================================================================

    float4 col0() const noexcept {
        return float4(row0.x, row1.x, row2.x, row3.x);
    }

    float4 col1() const noexcept {
        return float4(row0.y, row1.y, row2.y, row3.y);
    }

    float4 col2() const noexcept {
        return float4(row0.z, row1.z, row2.z, row3.z);
    }

    float4 col3() const noexcept {
        return float4(row0.w, row1.w, row2.w, row3.w);
    }

    void set_col0(const float4& col) noexcept {
        row0.x = col.x;
        row1.x = col.y;
        row2.x = col.z;
        row3.x = col.w;
    }

    void set_col1(const float4& col) noexcept {
        row0.y = col.x;
        row1.y = col.y;
        row2.y = col.z;
        row3.y = col.w;
    }

    void set_col2(const float4& col) noexcept {
        row0.z = col.x;
        row1.z = col.y;
        row2.z = col.z;
        row3.z = col.w;
    }

    void set_col3(const float4& col) noexcept {
        row0.w = col.x;
        row1.w = col.y;
        row2.w = col.z;
        row3.w = col.w;
    }

    // ============================================================================
    // Unary Operators
    // ============================================================================

    float4x4 operator+() const noexcept { return *this; }

    float4x4 operator-() const noexcept {
        return float4x4(-row0, -row1, -row2, -row3);
    }

    // ============================================================================
    // Static Constructors
    // ============================================================================

    static float4x4 identity() noexcept { return float4x4(); }
    static float4x4 zero() noexcept { return float4x4(0.0f); }

    static float4x4 translation(float x, float y, float z) noexcept {
        return float4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            x,    y,    z,    1.0f
        );
    }

    static float4x4 translation(const float3& t) noexcept {
        return translation(t.x, t.y, t.z);
    }

    static float4x4 scaling(float x, float y, float z) noexcept {
        return float4x4(x, 0.0f, 0.0f, 0.0f,
                        0.0f, y, 0.0f, 0.0f,
                        0.0f, 0.0f, z, 0.0f,
                        0.0f, 0.0f, 0.0f, 1.0f);
    }

    static float4x4 scaling(const float3& s) noexcept {
        return scaling(s.x, s.y, s.z);
    }

    static float4x4 scaling(float uniformScale) noexcept {
        return scaling(uniformScale, uniformScale, uniformScale);
    }

    static float4x4 rotation_x(float angle) noexcept;
    static float4x4 rotation_y(float angle) noexcept;
    static float4x4 rotation_z(float angle) noexcept;
    static float4x4 rotation_euler(const float3& angles) noexcept;

    static float4x4 perspective_lh_zo(float fovY, float aspect, float = 0.01f, float zFar = 100.0f) noexcept;
    static float4x4 perspective_rh_zo(float fovY, float aspect, float = 0.01f, float zFar = 100.0f) noexcept;
    static float4x4 perspective_lh_no(float fovY, float aspect, float = 0.01f, float zFar = 100.0f) noexcept;
    static float4x4 perspective(float fovY, float aspect, float zNear = 0.01f, float zFar = 100.0f) noexcept;
    static float4x4 orthographic_lh_zo(float width, float height, float zNear = 0.01f, float zFar = 100.0f) noexcept;
    static float4x4 orthographic_off_center_lh_zo(float left, float right, float bottom, float top, float zNear = 0.01f, float zFar = 100.0f) noexcept;
    static float4x4 orthographic(float w, float h, float zNear = 0.01f, float zFar = 100.0f) noexcept;

    static float4x4 look_at_lh(const float3& eye, const float3& target, const float3& up);
    static float4x4 look_at(const float3& eye, const float3& target, const float3& up);

    // ============================================================================
    // Utility Methods
    // ============================================================================

    std::string to_string() const {
        char buffer[512];
        std::snprintf(buffer, sizeof(buffer),
            "[%8.4f, %8.4f, %8.4f, %8.4f]\n"
            "[%8.4f, %8.4f, %8.4f, %8.4f]\n"
            "[%8.4f, %8.4f, %8.4f, %8.4f]\n"
            "[%8.4f, %8.4f, %8.4f, %8.4f]",
            row0.x, row0.y, row0.z, row0.w,
            row1.x, row1.y, row1.z, row1.w,
            row2.x, row2.y, row2.z, row2.w,
            row3.x, row3.y, row3.z, row3.w);
        return std::string(buffer);
    }

    void to_row_major(float* data) const noexcept {
        data[0] = row0.x; data[1] = row0.y; data[2] = row0.z; data[3] = row0.w;
        data[4] = row1.x; data[5] = row1.y; data[6] = row1.z; data[7] = row1.w;
        data[8] = row2.x; data[9] = row2.y; data[10] = row2.z; data[11] = row2.w;
        data[12] = row3.x; data[13] = row3.y; data[14] = row3.z; data[15] = row3.w;
    }

    void to_column_major(float* data) const noexcept {
        data[0] = row0.x; data[1] = row1.x; data[2] = row2.x; data[3] = row3.x;
        data[4] = row0.y; data[5] = row1.y; data[6] = row2.y; data[7] = row3.y;
        data[8] = row0.z; data[9] = row1.z; data[10] = row2.z; data[11] = row3.z;
        data[12] = row0.w; data[13] = row1.w; data[14] = row2.w; data[15] = row3.w;
    }
};

// ============================================================================
// Global Mathematical Functions (HLSL Style)
// ============================================================================

// Matrix operations
inline float4x4 operator+(const float4x4& a, const float4x4& b) noexcept {
    return float4x4(a.row0 + b.row0, a.row1 + b.row1, a.row2 + b.row2, a.row3 + b.row3);
}

inline float4x4 operator-(const float4x4& a, const float4x4& b) noexcept {
    return float4x4(a.row0 - b.row0, a.row1 - b.row1, a.row2 - b.row2, a.row3 - b.row3);
}

inline float4x4 operator*(const float4x4& mat, float scalar) noexcept {
    return float4x4(mat.row0 * scalar, mat.row1 * scalar, mat.row2 * scalar, mat.row3 * scalar);
}

inline float4x4 operator*(float scalar, const float4x4& mat) noexcept {
    return mat * scalar;
}

inline float4x4 operator/(const float4x4& mat, float scalar) noexcept {
    return float4x4(mat.row0 / scalar, mat.row1 / scalar, mat.row2 / scalar, mat.row3 / scalar);
}

// Matrix multiplication
inline float4x4 operator*(const float4x4& a, const float4x4& b) noexcept {
    float4x4 result;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a(i, k) * b(k, j);
            }
            result(i, j) = sum;
        }
    }

    return result;
}

// Matrix-vector multiplication
inline float4 operator*(const float4& vec, const float4x4& mat) noexcept
{
    float4 result;
    result.x = vec.x * mat.row0.x + vec.y * mat.row1.x + vec.z * mat.row2.x + vec.w * mat.row3.x;
    result.y = vec.x * mat.row0.y + vec.y * mat.row1.y + vec.z * mat.row2.y + vec.w * mat.row3.y;
    result.z = vec.x * mat.row0.z + vec.y * mat.row1.z + vec.z * mat.row2.z + vec.w * mat.row3.z;
    result.w = vec.x * mat.row0.w + vec.y * mat.row1.w + vec.z * mat.row2.w + vec.w * mat.row3.w;
    return result;
}

inline float3 operator*(const float3& point, const float4x4& mat) noexcept 
{
    float4 r = float4(point.x, point.y, point.z, 1.0f) * mat;
    return float3(r.x / r.w, r.y / r.w, r.z / r.w);
}

// Matrix properties and transformations
inline float4x4 transpose(const float4x4& mat) noexcept {
    __m128 t0 = _mm_shuffle_ps(mat.row0.get_simd(), mat.row1.get_simd(), 0x44);
    __m128 t2 = _mm_shuffle_ps(mat.row0.get_simd(), mat.row1.get_simd(), 0xEE);
    __m128 t1 = _mm_shuffle_ps(mat.row2.get_simd(), mat.row3.get_simd(), 0x44);
    __m128 t3 = _mm_shuffle_ps(mat.row2.get_simd(), mat.row3.get_simd(), 0xEE);

    __m128 row0 = _mm_shuffle_ps(t0, t1, 0x88);
    __m128 row1 = _mm_shuffle_ps(t0, t1, 0xDD);
    __m128 row2 = _mm_shuffle_ps(t2, t3, 0x88);
    __m128 row3 = _mm_shuffle_ps(t2, t3, 0xDD);

    return float4x4(float4(row0), float4(row1), float4(row2), float4(row3));
}

inline float determinant(const float4x4& mat) noexcept {
    float a = mat.row0.x, b = mat.row0.y, c = mat.row0.z, d = mat.row0.w;
    float e = mat.row1.x, f = mat.row1.y, g = mat.row1.z, h = mat.row1.w;
    float i = mat.row2.x, j = mat.row2.y, k = mat.row2.z, l = mat.row2.w;
    float m = mat.row3.x, n = mat.row3.y, o = mat.row3.z, p = mat.row3.w;

    float kplo = k * p - l * o;
    float jpln = j * p - l * n;
    float jokn = j * o - k * n;
    float iplm = i * p - l * m;
    float iokm = i * o - k * m;
    float in_jm = i * n - j * m;

    return  a * (f * kplo - g * jpln + h * jokn) -
            b * (e * kplo - g * iplm + h * iokm) +
            c * (e * jpln - f * iplm + h * in_jm) -
            d * (e * jokn - f * iokm + g * in_jm);
}

inline float4x4 inverse_affine(const float4x4& mat) noexcept {
    const float3 r0 = float3(mat.row0.x, mat.row0.y, mat.row0.z);
    const float3 r1 = float3(mat.row1.x, mat.row1.y, mat.row1.z);
    const float3 r2 = float3(mat.row2.x, mat.row2.y, mat.row2.z);
    const float3 t = float3(mat.row3.x, mat.row3.y, mat.row3.z);

    const float det = dot(r0, cross(r1, r2));
    if (std::abs(det) < 1e-8f) {
        return float4x4::identity();
    }

    const float inv_det = 1.0f / det;

    const float3 c0 = cross(r1, r2) * inv_det;
    const float3 c1 = cross(r2, r0) * inv_det;
    const float3 c2 = cross(r0, r1) * inv_det;

    float3 inv_t = float3(-dot(c0, t), -dot(c1, t), -dot(c2, t));

    return float4x4(
        c0.x,    c1.x,    c2.x,    0.0f,
        c0.y,    c1.y,    c2.y,    0.0f,
        c0.z,    c1.z,    c2.z,    0.0f,
        inv_t.x, inv_t.y, inv_t.z, 1.0f
    );
}

inline float4x4 adjugate(const float4x4& mat) noexcept {
    float a = mat.row0.x, b = mat.row0.y, c = mat.row0.z, d = mat.row0.w;
    float e = mat.row1.x, f = mat.row1.y, g = mat.row1.z, h = mat.row1.w;
    float i = mat.row2.x, j = mat.row2.y, k = mat.row2.z, l = mat.row2.w;
    float m = mat.row3.x, n = mat.row3.y, o = mat.row3.z, p = mat.row3.w;

    float kplo = k * p - l * o;
    float jpln = j * p - l * n;
    float jokn = j * o - k * n;
    float iplm = i * p - l * m;
    float iokm = i * o - k * m;
    float in_jm = i * n - j * m;
    float gpho = g * p - h * o;
    float fphn = f * p - h * n;
    float fogn = f * o - g * n;
    float ep_hm = e * p - h * m;
    float eogm = e * o - g * m;
    float en_fm = e * n - f * m;
    float gl_hk = g * l - h * k;
    float fl_hj = f * l - h * j;
    float fk_gj = f * k - g * j;
    float el_hi = e * l - h * i;
    float ek_gi = e * k - g * i;
    float ej_fi = e * j - f * i;

    return float4x4(f * kplo - g * jpln + h * jokn,
                    -b * kplo + c * jpln - d * jokn,
                    b * gpho - c * fphn + d * fogn,
                    -b * gl_hk + c * fl_hj - d * fk_gj,
                    -e * kplo + g * iplm - h * iokm,
                    a * kplo - c * iplm + d * iokm,
                    -a * gpho + c * ep_hm - d * eogm,
                    a * gl_hk - c * el_hi + d * ek_gi,
                    e * jpln - f * iplm + h * in_jm,
                    -a * jpln + b * iplm - d * in_jm,
                    a * fphn - b * ep_hm + d * en_fm,
                    -a * fl_hj + b * el_hi - d * ej_fi,
                    -e * jokn + f * iokm - g * in_jm,
                    a * jokn - b * iokm + c * in_jm,
                    -a * fogn + b * eogm - c * en_fm,
                    a * fk_gj - b * ek_gi + c * ej_fi);
}

inline bool is_affine(const float4x4& mat, float epsilon = 1e-6f) noexcept {
    return std::abs(mat.row0.w) < epsilon &&
           std::abs(mat.row1.w) < epsilon &&
           std::abs(mat.row2.w) < epsilon &&
           std::abs(mat.row3.w - 1.0f) < epsilon;
}

inline float4x4 inverse(const float4x4& mat) noexcept {
    if (is_affine(mat)) {
        return inverse_affine(mat);
    }

    float det = determinant(mat);
    if (std::abs(det) < 1e-8f) {
        return float4x4::identity();
    }

    return adjugate(mat) / det;
}

inline float trace(const float4x4& mat) noexcept {
    return mat.row0.x + mat.row1.y + mat.row2.z + mat.row3.w;
}

inline float4 diagonal(const float4x4& mat) noexcept {
    return float4(mat.row0.x, mat.row1.y, mat.row2.z, mat.row3.w);
}

inline float frobenius_norm(const float4x4& mat) noexcept {
    return std::sqrt(length_sq(mat.row0) + length_sq(mat.row1) + length_sq(mat.row2) + length_sq(mat.row3));
}

// Vector transformations
inline float4 transform_vector(const float4x4& mat, const float4& vec) noexcept {
    return vec * mat;
}

inline float3 transform_point(const float4x4& mat, const float3& point) noexcept {
    float4 r = float4(point.x, point.y, point.z, 1.0f) * mat;
    return float3(r.x / r.w, r.y / r.w, r.z / r.w);
}

inline float3 transform_vector(const float4x4& mat, const float3& vec) noexcept {
    float4 r = float4(vec.x, vec.y, vec.z, 0.0f) * mat;
    return float3(r.x, r.y, r.z);
}

inline float3 transform_direction(const float4x4& mat, const float3& dir) noexcept {
    return normalize(transform_vector(mat, dir));
}

// Matrix properties
inline float3 get_translation(const float4x4& mat) noexcept {
    return float3(mat.row3.x, mat.row3.y, mat.row3.z);
}

inline float3 get_scale(const float4x4& mat) noexcept {
    float3 col0 = float3(mat.row0.x, mat.row0.y, mat.row0.z);
    float3 col1 = float3(mat.row1.x, mat.row1.y, mat.row1.z);
    float3 col2 = float3(mat.row2.x, mat.row2.y, mat.row2.z);
    return float3(length(col0), length(col1), length(col2));
}

inline bool is_identity(const float4x4& mat, float epsilon = 1e-6f) noexcept {
    return  approximately(mat.row0, float4(1.0f, 0.0f, 0.0f, 0.0f), epsilon) &&
            approximately(mat.row1, float4(0.0f, 1.0f, 0.0f, 0.0f), epsilon) &&
            approximately(mat.row2, float4(0.0f, 0.0f, 1.0f, 0.0f), epsilon) &&
            approximately(mat.row3, float4(0.0f, 0.0f, 0.0f, 1.0f), epsilon);
}

inline bool is_orthogonal(const float4x4& mat, float epsilon = 1e-6f) noexcept {
    if (!is_affine(mat, epsilon)) return false;

    float3 r0 = float3(mat.row0.x, mat.row0.y, mat.row0.z);
    float3 r1 = float3(mat.row1.x, mat.row1.y, mat.row1.z);
    float3 r2 = float3(mat.row2.x, mat.row2.y, mat.row2.z);

    float dot01 = std::abs(dot(r0, r1));
    float dot02 = std::abs(dot(r0, r2));
    float dot12 = std::abs(dot(r1, r2));

    if (dot01 > epsilon || dot02 > epsilon || dot12 > epsilon) {
        return false;
    }

    float len0 = length_sq(r0);
    float len1 = length_sq(r1);
    float len2 = length_sq(r2);

    return  approximately(len0, 1.0f, epsilon) &&
            approximately(len1, 1.0f, epsilon) &&
            approximately(len2, 1.0f, epsilon);
}

inline bool approximately(const float4x4& a, const float4x4& b, float epsilon = 1e-6f) noexcept {
    return  approximately(a.row0, b.row0, epsilon) &&
            approximately(a.row1, b.row1, epsilon) &&
            approximately(a.row2, b.row2, epsilon) &&
            approximately(a.row3, b.row3, epsilon);
}

inline bool approximately_zero(const float4x4& mat, float epsilon = 1e-6f) noexcept {
    return approximately(mat, float4x4::zero(), epsilon);
}

// HLSL-like functions
inline float4 mul(const float4& vec, const float4x4& mat) noexcept {
    return vec * mat;
}

inline float3 mul(const float3& point, const float4x4& mat) noexcept {
    return transform_point(mat, point);
}

inline float4x4 float4x4::rotation_x(float angle) noexcept
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return float4x4(1,  0, 0, 0,
                    0,  c, s, 0,
                    0, -s, c, 0,
                    0,  0, 0, 1);
}
inline float4x4 float4x4::rotation_y(float angle) noexcept
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return float4x4(c, 0, -s, 0,
                    0, 1,  0, 0,
                    s, 0,  c, 0,
                    0, 0,  0, 1);
}
inline float4x4 float4x4::rotation_z(float angle) noexcept
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    return float4x4( c, s, 0, 0,
                    -s, c, 0, 0,
                     0, 0, 1, 0,
                     0, 0, 0, 1);
}

inline float4x4 float4x4::rotation_euler(const float3& angles) noexcept {
    return rotation_z(angles.z) * rotation_y(angles.y) * rotation_x(angles.x);
}

inline float4x4 float4x4::perspective_lh_zo(float fovY, float aspect, float zNear, float zFar) noexcept {
    float cot = 1.0f / std::tan(fovY * 0.5f);
    float r = zFar / (zFar - zNear);
    return float4x4(
        cot / aspect, 0.0f, 0.0f,       0.0f,
        0.0f,         cot,  0.0f,       0.0f,
        0.0f,         0.0f, r,          1.0f,
        0.0f,         0.0f, -r * zNear, 0.0f
    );
}

inline float4x4 float4x4::perspective_lh_no(float fovY, float aspect, float zNear, float zFar) noexcept {
    float cot = 1.0f / std::tan(fovY * 0.5f);
    float w = cot / aspect;
    float r = zFar / (zFar - zNear);
    return float4x4(
        w,    0.0f, 0.0f,       0.0f,
        0.0f, cot,  0.0f,       0.0f,
        0.0f, 0.0f, r,          1.0f,
        0.0f, 0.0f, -r * zNear, 0.0f
    );
}

inline float4x4 float4x4::perspective(float fovY, float aspect, float zNear, float zFar) noexcept {
    return perspective_lh_zo(fovY, aspect, zNear, zFar);
}

inline float4x4 float4x4::orthographic_lh_zo(float width, float height, float zNear, float zFar) noexcept {
    float r = 1.0f / (zFar - zNear);
    return float4x4(
        2.0f / width, 0.0f,          0.0f,       0.0f,
        0.0f,         2.0f / height, 0.0f,       0.0f,
        0.0f,         0.0f,          r,          0.0f,
        0.0f,         0.0f,          -zNear * r, 1.0f
    );
}

inline float4x4 float4x4::orthographic_off_center_lh_zo(float left, float right, float bottom, float top, float zNear, float zFar) noexcept {
    float fRange = 1.0f / (zFar - zNear);
    return float4x4(
        2.0f / (right - left),            0.0f,                             0.0f,            0.0f,
        0.0f,                             2.0f / (top - bottom),            0.0f,            0.0f,
        0.0f,                             0.0f,                             fRange,          0.0f,
        -(left + right) / (right - left), -(top + bottom) / (top - bottom), -zNear * fRange, 1.0f);
}

inline float4x4 float4x4::orthographic(float w, float h, float zn, float zf) noexcept {
    return orthographic_lh_zo(w, h, zn, zf);
}

inline float4x4 float4x4::look_at_lh(const float3& eye, const float3& target, const float3& up) {
    float3 zaxis = normalize(target - eye);
    float3 xaxis = normalize(cross(up, zaxis));
    float3 yaxis = cross(zaxis, xaxis);

    return float4x4(
        xaxis.x, xaxis.y, xaxis.z, 0,
        yaxis.x, yaxis.y, yaxis.z, 0,
        zaxis.x, zaxis.y, zaxis.z, 0,
        -dot(xaxis, eye), -dot(yaxis, eye), -dot(zaxis, eye), 1
    );
}

inline float4x4 float4x4::look_at(const float3& eye, const float3& target, const float3& up) {
    return look_at_lh(eye, target, up);
}

// ============================================================================
// Global Transformation Functions (HLSL Style)
// ============================================================================

// Translation
inline float4x4 translation(float x, float y, float z) noexcept {
    return float4x4::translation(x, y, z);
}

inline float4x4 translation(const float3& t) noexcept {
    return float4x4::translation(t);
}

inline float4x4 translation(float scalar) noexcept
{
    return float4x4::translation(scalar, scalar, scalar);
}

// Scaling
inline float4x4 scaling(float x, float y, float z) noexcept {
    return float4x4::scaling(x, y, z);
}

inline float4x4 scaling(const float3& s) noexcept {
    return float4x4::scaling(s);
}

inline float4x4 scaling(float uniformScale) noexcept {
    return float4x4::scaling(uniformScale);
}

// Rotation around principal axes
inline float4x4 rotation_x(float angle) noexcept {
    return float4x4::rotation_x(angle);
}

inline float4x4 rotation_y(float angle) noexcept {
    return float4x4::rotation_y(angle);
}

inline float4x4 rotation_z(float angle) noexcept {
    return float4x4::rotation_z(angle);
}

inline float4x4 rotation_euler(const float3& angles) noexcept {
    return float4x4::rotation_euler(angles);
}

// Perspective projections
inline float4x4 perspective_lh_zo(float fovY, float aspect, float zNear = 0.01f, float zFar = 100.0f) noexcept {
    return float4x4::perspective_lh_zo(fovY, aspect, zNear, zFar);
}

inline float4x4 perspective_lh_no(float fovY, float aspect, float zNear = 0.01f, float zFar = 100.0f) noexcept {
    return float4x4::perspective_lh_no(fovY, aspect, zNear, zFar);
}

inline float4x4 perspective(float fovY, float aspect, float zNear = 0.01f, float zFar = 100.0f) noexcept {
    return float4x4::perspective(fovY, aspect, zNear, zFar);
}

// Orthographic projections
inline float4x4 orthographic_lh_zo(float width, float height, float zNear = 0.01f, float zFar = 100.0f) noexcept {
    return float4x4::orthographic_lh_zo(width, height, zNear, zFar);
}

inline float4x4 orthographic_off_center_lh_zo(float left, float right, float bottom, float top, float zNear = 0.01f, float zFar = 100.0f) noexcept {
    return float4x4::orthographic_off_center_lh_zo(left, right, bottom, top, zNear, zFar);
}

inline float4x4 orthographic(float w, float h, float, float zNear = 0.01f, float zFar = 100.0f) noexcept {
    return float4x4::orthographic(w, h, zNear, zFar);
}

// Additional global functions
inline float4x4 rotation_axis(const float3& axis, float angle) noexcept {
    if (approximately_zero(axis, 1e-8f)) {
        return float4x4::identity();
    }

    float s = std::sin(angle);
    float c = std::cos(angle);
    float t = 1.0f - c;
    float3 n = normalize(axis);
    float x = n.x, y = n.y, z = n.z;

    return float4x4(t * x * x + c, t * x * y + z * s, t * x * z - y * s, 0.0f,
                    t * x * y - z * s, t * y * y + c, t * y * z + x * s, 0.0f,
                    t * x * z + y * s, t * y * z - x * s, t * z * z + c, 0.0f,
                    0.0f,              0.0f,              0.0f,          1.0f);
}

inline float4x4 look_at_lh(const float3& eye = float3(0.0f, 0.0f, 0.0f), const float3& target = float3(0.0f, 0.0f, 0.0f), const float3& up = float3(0.0f, 1.0f, 0.0f)) noexcept {
    return float4x4::look_at_lh(eye, target, up);
}

inline float4x4 look_at(const float3& eye = float3(0.0f, 0.0f, 0.0f), const float3& target = float3(0.0f, 0.0f, 0.0f), const float3& up = float3(0.0f, 1.0f, 0.0f)) noexcept {
    return look_at_lh(eye, target, up);
}

// Comparison operators
inline bool operator==(const float4x4& a, const float4x4& b) noexcept {
    return approximately(a, b);
}

inline bool operator!=(const float4x4& a, const float4x4& b) noexcept {
    return !approximately(a, b);
}

// ============================================================================

inline float4x4 operator*(const float3x3& a, const float4x4& b) noexcept {
    float4x4 result;

    for (int i = 0; i < 3; ++i) {
        float4 row;
        for (int j = 0; j < 4; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                sum += a[i][k] * b[k][j];
            }
            row[j] = sum;
        }
        result[i] = row;
    }

    result[3] = b[3];

    return result;
}

inline float4x4 operator*(const float4x4& a, const float3x3& b) noexcept {
    float4x4 result;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < 3; ++k) {
                sum += a[i][k] * b[k][j];
            }
            result[i][j] = sum;
        }
        result[i][3] = a[i][3];
    }

    return result;
}

// ============================================================================
// Useful Constants
// ============================================================================

AFTERMATH_INLINE_VAR const float4x4 float4x4_Identity = float4x4::identity();
AFTERMATH_INLINE_VAR const float4x4 float4x4_Zero = float4x4::zero();

AFTERMATH_END
