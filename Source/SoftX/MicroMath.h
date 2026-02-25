#pragma once

#include <immintrin.h>   // SSE4.2
#include <cmath>         // для fabs, sqrt, sinf, cosf, tanf
#include <algorithm>     // для std::min, std::max
#include <cstdlib>       // для std::abs (int)

#undef min
#undef max

// ==================== Константы и угловые преобразования ====================
constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

inline float DegToRad(float degrees) { return degrees * DEG_TO_RAD; }
inline float RadToDeg(float radians) { return radians * RAD_TO_DEG; }

// ==================== float2 ====================
struct float2 {
    float x, y;

    // Конструкторы
    float2() : x(0), y(0) {}
    float2(float x, float y) : x(x), y(y) {}
    float2(const float2&) = default;

    // Присваивание
    float2& operator=(const float2&) = default;

    // Доступ по индексу
    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }

    // Унарный минус
    float2 operator-() const { return float2(-x, -y); }

    // Арифметические операторы (покомпонентно)
    float2 operator+(const float2& other) const { return float2(x + other.x, y + other.y); }
    float2 operator-(const float2& other) const { return float2(x - other.x, y - other.y); }
    float2 operator*(const float2& other) const { return float2(x * other.x, y * other.y); }
    float2 operator/(const float2& other) const { return float2(x / other.x, y / other.y); }

    // Составные присваивания
    float2& operator+=(const float2& other) { x += other.x; y += other.y; return *this; }
    float2& operator-=(const float2& other) { x -= other.x; y -= other.y; return *this; }
    float2& operator*=(const float2& other) { x *= other.x; y *= other.y; return *this; }
    float2& operator/=(const float2& other) { x /= other.x; y /= other.y; return *this; }

    // Скалярные операции
    float2 operator+(float s) const { return float2(x + s, y + s); }
    float2 operator-(float s) const { return float2(x - s, y - s); }
    float2 operator*(float s) const { return float2(x * s, y * s); }
    float2 operator/(float s) const { return float2(x / s, y / s); }

    float2& operator+=(float s) { x += s; y += s; return *this; }
    float2& operator-=(float s) { x -= s; y -= s; return *this; }
    float2& operator*=(float s) { x *= s; y *= s; return *this; }
    float2& operator/=(float s) { x /= s; y /= s; return *this; }

    // Операторы сравнения (точное сравнение, осторожно)
    bool operator==(const float2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const float2& other) const { return !(*this == other); }
};

// Скалярные операции слева
inline float2 operator+(float s, const float2& v) { return v + s; }
inline float2 operator-(float s, const float2& v) { return float2(s - v.x, s - v.y); }
inline float2 operator*(float s, const float2& v) { return v * s; }
inline float2 operator/(float s, const float2& v) { return float2(s / v.x, s / v.y); }

// Алгебраические функции для float2
inline float dot(const float2& a, const float2& b) { return a.x * b.x + a.y * b.y; }
inline float length(const float2& v) { return std::sqrt(dot(v, v)); }
inline float2 normalize(const float2& v) {
    float len = length(v);
    return (len > 1e-6f) ? v * (1.0f / len) : float2(0, 0);
}
inline float2 min(const float2& a, const float2& b) { return float2(std::min(a.x, b.x), std::min(a.y, b.y)); }
inline float2 max(const float2& a, const float2& b) { return float2(std::max(a.x, b.x), std::max(a.y, b.y)); }
inline float2 abs(const float2& v) { return float2(std::abs(v.x), std::abs(v.y)); }
inline float cross(const float2& a, const float2& b) { return a.x * b.y - a.y * b.x; }

// ==================== float3 ====================
struct float3 {
    float x, y, z;

    // Конструкторы
    float3() : x(0), y(0), z(0) {}
    float3(float x, float y, float z) : x(x), y(y), z(z) {}
    float3(const float3&) = default;

    // Присваивание
    float3& operator=(const float3&) = default;

    // Доступ по индексу
    float& operator[](int i) { return (&x)[i]; }
    const float& operator[](int i) const { return (&x)[i]; }

    // Унарный минус
    float3 operator-() const { return float3(-x, -y, -z); }

    // Арифметические операторы (покомпонентно)
    float3 operator+(const float3& other) const { return float3(x + other.x, y + other.y, z + other.z); }
    float3 operator-(const float3& other) const { return float3(x - other.x, y - other.y, z - other.z); }
    float3 operator*(const float3& other) const { return float3(x * other.x, y * other.y, z * other.z); }
    float3 operator/(const float3& other) const { return float3(x / other.x, y / other.y, z / other.z); }

    // Составные присваивания
    float3& operator+=(const float3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    float3& operator-=(const float3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    float3& operator*=(const float3& other) { x *= other.x; y *= other.y; z *= other.z; return *this; }
    float3& operator/=(const float3& other) { x /= other.x; y /= other.y; z /= other.z; return *this; }

    // Скалярные операции
    float3 operator+(float s) const { return float3(x + s, y + s, z + s); }
    float3 operator-(float s) const { return float3(x - s, y - s, z - s); }
    float3 operator*(float s) const { return float3(x * s, y * s, z * s); }
    float3 operator/(float s) const { return float3(x / s, y / s, z / s); }

    float3& operator+=(float s) { x += s; y += s; z += s; return *this; }
    float3& operator-=(float s) { x -= s; y -= s; z -= s; return *this; }
    float3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float3& operator/=(float s) { x /= s; y /= s; z /= s; return *this; }

    // Операторы сравнения
    bool operator==(const float3& other) const { return x == other.x && y == other.y && z == other.z; }
    bool operator!=(const float3& other) const { return !(*this == other); }
};

// Скалярные операции слева
inline float3 operator+(float s, const float3& v) { return v + s; }
inline float3 operator-(float s, const float3& v) { return float3(s - v.x, s - v.y, s - v.z); }
inline float3 operator*(float s, const float3& v) { return v * s; }
inline float3 operator/(float s, const float3& v) { return float3(s / v.x, s / v.y, s / v.z); }

// Алгебраические функции для float3
inline float dot(const float3& a, const float3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline float3 cross(const float3& a, const float3& b) {
    return float3(a.y * b.z - a.z * b.y,
                  a.z * b.x - a.x * b.z,
                  a.x * b.y - a.y * b.x);
}
inline float length(const float3& v) { return std::sqrt(dot(v, v)); }
inline float3 normalize(const float3& v) {
    float len = length(v);
    return (len > 1e-6f) ? v * (1.0f / len) : float3(0, 0, 0);
}
inline float3 min(const float3& a, const float3& b) {
    return float3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}
inline float3 max(const float3& a, const float3& b) {
    return float3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}
inline float3 abs(const float3& v) { return float3(std::abs(v.x), std::abs(v.y), std::abs(v.z)); }

// ==================== int2 ====================
struct int2 {
    int x, y;

    int2() : x(0), y(0) {}
    int2(int x, int y) : x(x), y(y) {}
    int2(const int2&) = default;

    int2& operator=(const int2&) = default;

    int& operator[](int i) { return (&x)[i]; }
    const int& operator[](int i) const { return (&x)[i]; }

    int2 operator-() const { return int2(-x, -y); }

    // Арифметика
    int2 operator+(const int2& other) const { return int2(x + other.x, y + other.y); }
    int2 operator-(const int2& other) const { return int2(x - other.x, y - other.y); }
    int2 operator*(const int2& other) const { return int2(x * other.x, y * other.y); }
    int2 operator/(const int2& other) const { return int2(x / other.x, y / other.y); }
    int2 operator%(const int2& other) const { return int2(x % other.x, y % other.y); }

    int2& operator+=(const int2& other) { x += other.x; y += other.y; return *this; }
    int2& operator-=(const int2& other) { x -= other.x; y -= other.y; return *this; }
    int2& operator*=(const int2& other) { x *= other.x; y *= other.y; return *this; }
    int2& operator/=(const int2& other) { x /= other.x; y /= other.y; return *this; }
    int2& operator%=(const int2& other) { x %= other.x; y %= other.y; return *this; }

    // Скалярные операции
    int2 operator+(int s) const { return int2(x + s, y + s); }
    int2 operator-(int s) const { return int2(x - s, y - s); }
    int2 operator*(int s) const { return int2(x * s, y * s); }
    int2 operator/(int s) const { return int2(x / s, y / s); }
    int2 operator%(int s) const { return int2(x % s, y % s); }

    int2& operator+=(int s) { x += s; y += s; return *this; }
    int2& operator-=(int s) { x -= s; y -= s; return *this; }
    int2& operator*=(int s) { x *= s; y *= s; return *this; }
    int2& operator/=(int s) { x /= s; y /= s; return *this; }
    int2& operator%=(int s) { x %= s; y %= s; return *this; }

    // Сравнения (покомпонентно)
    bool operator==(const int2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const int2& other) const { return !(*this == other); }
    bool operator<(const int2& other) const { return x < other.x && y < other.y; }
    bool operator<=(const int2& other) const { return x <= other.x && y <= other.y; }
    bool operator>(const int2& other) const { return x > other.x && y > other.y; }
    bool operator>=(const int2& other) const { return x >= other.x && y >= other.y; }
};

// Скалярные операции слева для int2
inline int2 operator+(int s, const int2& v) { return v + s; }
inline int2 operator-(int s, const int2& v) { return int2(s - v.x, s - v.y); }
inline int2 operator*(int s, const int2& v) { return v * s; }
inline int2 operator/(int s, const int2& v) { return int2(s / v.x, s / v.y); }
inline int2 operator%(int s, const int2& v) { return int2(s % v.x, s % v.y); }

// Дополнительные функции для int2
inline int2 min(const int2& a, const int2& b) { return int2(std::min(a.x, b.x), std::min(a.y, b.y)); }
inline int2 max(const int2& a, const int2& b) { return int2(std::max(a.x, b.x), std::max(a.y, b.y)); }
inline int2 abs(const int2& v) { return int2(std::abs(v.x), std::abs(v.y)); }
inline int dot(const int2& a, const int2& b) { return a.x * b.x + a.y * b.y; }
inline int cross(const int2& a, const int2& b) { return a.x * b.y - a.y * b.x; }

// ==================== int3 ====================
struct int3 {
    int x, y, z;

    int3() : x(0), y(0), z(0) {}
    int3(int x, int y, int z) : x(x), y(y), z(z) {}
    int3(const int3&) = default;

    int3& operator=(const int3&) = default;

    int& operator[](int i) { return (&x)[i]; }
    const int& operator[](int i) const { return (&x)[i]; }

    int3 operator-() const { return int3(-x, -y, -z); }

    // Арифметика
    int3 operator+(const int3& other) const { return int3(x + other.x, y + other.y, z + other.z); }
    int3 operator-(const int3& other) const { return int3(x - other.x, y - other.y, z - other.z); }
    int3 operator*(const int3& other) const { return int3(x * other.x, y * other.y, z * other.z); }
    int3 operator/(const int3& other) const { return int3(x / other.x, y / other.y, z / other.z); }
    int3 operator%(const int3& other) const { return int3(x % other.x, y % other.y, z % other.z); }

    int3& operator+=(const int3& other) { x += other.x; y += other.y; z += other.z; return *this; }
    int3& operator-=(const int3& other) { x -= other.x; y -= other.y; z -= other.z; return *this; }
    int3& operator*=(const int3& other) { x *= other.x; y *= other.y; z *= other.z; return *this; }
    int3& operator/=(const int3& other) { x /= other.x; y /= other.y; z /= other.z; return *this; }
    int3& operator%=(const int3& other) { x %= other.x; y %= other.y; z %= other.z; return *this; }

    // Скалярные операции
    int3 operator+(int s) const { return int3(x + s, y + s, z + s); }
    int3 operator-(int s) const { return int3(x - s, y - s, z - s); }
    int3 operator*(int s) const { return int3(x * s, y * s, z * s); }
    int3 operator/(int s) const { return int3(x / s, y / s, z / s); }
    int3 operator%(int s) const { return int3(x % s, y % s, z % s); }

    int3& operator+=(int s) { x += s; y += s; z += s; return *this; }
    int3& operator-=(int s) { x -= s; y -= s; z -= s; return *this; }
    int3& operator*=(int s) { x *= s; y *= s; z *= s; return *this; }
    int3& operator/=(int s) { x /= s; y /= s; z /= s; return *this; }
    int3& operator%=(int s) { x %= s; y %= s; z %= s; return *this; }

    // Сравнения (покомпонентно)
    bool operator==(const int3& other) const { return x == other.x && y == other.y && z == other.z; }
    bool operator!=(const int3& other) const { return !(*this == other); }
    bool operator<(const int3& other) const { return x < other.x && y < other.y && z < other.z; }
    bool operator<=(const int3& other) const { return x <= other.x && y <= other.y && z <= other.z; }
    bool operator>(const int3& other) const { return x > other.x && y > other.y && z > other.z; }
    bool operator>=(const int3& other) const { return x >= other.x && y >= other.y && z >= other.z; }
};

// Скалярные операции слева для int3
inline int3 operator+(int s, const int3& v) { return v + s; }
inline int3 operator-(int s, const int3& v) { return int3(s - v.x, s - v.y, s - v.z); }
inline int3 operator*(int s, const int3& v) { return v * s; }
inline int3 operator/(int s, const int3& v) { return int3(s / v.x, s / v.y, s / v.z); }
inline int3 operator%(int s, const int3& v) { return int3(s % v.x, s % v.y, s % v.z); }

// Дополнительные функции для int3
inline int3 min(const int3& a, const int3& b) {
    return int3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}
inline int3 max(const int3& a, const int3& b) {
    return int3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}
inline int3 abs(const int3& v) { return int3(std::abs(v.x), std::abs(v.y), std::abs(v.z)); }
inline int dot(const int3& a, const int3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline int3 cross(const int3& a, const int3& b) {
    return int3(a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x);
}

// ==================== float4 (SSE) ====================
class alignas(16) float4 {
public:
    union {
        __m128 v;            // SSE-вектор
        struct { float x, y, z, w; };
    };

    // Конструкторы
    float4() : v(_mm_setzero_ps()) {}
    explicit float4(__m128 vec) : v(vec) {}
    float4(float x, float y, float z, float w) : v(_mm_set_ps(w, z, y, x)) {}
    float4(const float4& other) : v(other.v) {}

    // Присваивание
    float4& operator=(const float4& other) { v = other.v; return *this; }
    float4& operator=(__m128 vec) { v = vec; return *this; }

    // Доступ по индексу
    float& operator[](int index) { return (&x)[index]; }
    const float& operator[](int index) const { return (&x)[index]; }

    // Унарный минус
    float4 operator-() const { return float4(_mm_sub_ps(_mm_setzero_ps(), v)); }

    // Арифметические операторы (покомпонентно)
    float4 operator+(const float4& other) const { return float4(_mm_add_ps(v, other.v)); }
    float4 operator-(const float4& other) const { return float4(_mm_sub_ps(v, other.v)); }
    float4 operator*(const float4& other) const { return float4(_mm_mul_ps(v, other.v)); }
    float4 operator/(const float4& other) const { return float4(_mm_div_ps(v, other.v)); }

    // Составные присваивания
    float4& operator+=(const float4& other) { v = _mm_add_ps(v, other.v); return *this; }
    float4& operator-=(const float4& other) { v = _mm_sub_ps(v, other.v); return *this; }
    float4& operator*=(const float4& other) { v = _mm_mul_ps(v, other.v); return *this; }
    float4& operator/=(const float4& other) { v = _mm_div_ps(v, other.v); return *this; }

    // Операторы сравнения
    bool operator==(const float4& other) const {
        return _mm_movemask_ps(_mm_cmpeq_ps(v, other.v)) == 0xF;
    }
    bool operator!=(const float4& other) const { return !(*this == other); }

    // Скалярные операции
    float4 operator+(float s) const { return float4(_mm_add_ps(v, _mm_set1_ps(s))); }
    float4 operator-(float s) const { return float4(_mm_sub_ps(v, _mm_set1_ps(s))); }
    float4 operator*(float s) const { return float4(_mm_mul_ps(v, _mm_set1_ps(s))); }
    float4 operator/(float s) const { return float4(_mm_div_ps(v, _mm_set1_ps(s))); }

    float4& operator+=(float s) { v = _mm_add_ps(v, _mm_set1_ps(s)); return *this; }
    float4& operator-=(float s) { v = _mm_sub_ps(v, _mm_set1_ps(s)); return *this; }
    float4& operator*=(float s) { v = _mm_mul_ps(v, _mm_set1_ps(s)); return *this; }
    float4& operator/=(float s) { v = _mm_div_ps(v, _mm_set1_ps(s)); return *this; }

    // Дружественные функции для скалярных операций слева
    friend float4 operator+(float s, const float4& v) { return v + s; }
    friend float4 operator-(float s, const float4& v) { return float4(_mm_sub_ps(_mm_set1_ps(s), v.v)); }
    friend float4 operator*(float s, const float4& v) { return v * s; }
    friend float4 operator/(float s, const float4& v) { return float4(_mm_div_ps(_mm_set1_ps(s), v.v)); }
};

// Математические функции для float4
inline float dot(const float4& a, const float4& b) {
    __m128 mul = _mm_mul_ps(a.v, b.v);
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ps(sums, shuf);
    float result;
    _mm_store_ss(&result, sums);
    return result;
}

inline float4 cross(const float4& a, const float4& b) {
    __m128 a_yzx = _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_yzx = _mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 a_zxy = _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_zxy = _mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 mul1 = _mm_mul_ps(a_yzx, b_zxy);
    __m128 mul2 = _mm_mul_ps(a_zxy, b_yzx);
    __m128 result = _mm_sub_ps(mul1, mul2);
    result = _mm_blend_ps(result, _mm_setzero_ps(), 0b1000);
    return float4(result);
}

inline float length(const float4& v) { return std::sqrt(dot(v, v)); }
inline float4 normalize(const float4& v) {
    float len = length(v);
    return (len > 1e-6f) ? v * (1.0f / len) : float4(0, 0, 0, 0);
}
inline float4 min(const float4& a, const float4& b) { return float4(_mm_min_ps(a.v, b.v)); }
inline float4 max(const float4& a, const float4& b) { return float4(_mm_max_ps(a.v, b.v)); }
inline float4 abs(const float4& v) { return float4(_mm_andnot_ps(_mm_set1_ps(-0.0f), v.v)); }

inline float edgeFunction(const float4& a, const float4& b, const float2& c)
{
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}
inline float edgeFunction(const float4& a, const float4& b, const float4& c)
{
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

// ==================== float4x4 (Row-major, SSE) ====================
class alignas(16) float4x4 {
public:
    float4 r0, r1, r2, r3;  // строки

    // Конструкторы
    float4x4() : r0(1,0,0,0), r1(0,1,0,0), r2(0,0,1,0), r3(0,0,0,1) {} // identity
    explicit float4x4(const float4& row0, const float4& row1, const float4& row2, const float4& row3)
        : r0(row0), r1(row1), r2(row2), r3(row3) {}
    float4x4(float m00, float m01, float m02, float m03,
             float m10, float m11, float m12, float m13,
             float m20, float m21, float m22, float m23,
             float m30, float m31, float m32, float m33)
        : r0(m00, m01, m02, m03)
        , r1(m10, m11, m12, m13)
        , r2(m20, m21, m22, m23)
        , r3(m30, m31, m32, m33) {}
    float4x4(const float4x4&) = default;

    // Присваивание
    float4x4& operator=(const float4x4&) = default;

    // Доступ к строкам
    float4& operator[](int row) { return (&r0)[row]; }
    const float4& operator[](int row) const { return (&r0)[row]; }

    // Доступ к элементам (строка, столбец)
    float& operator()(int row, int col) { return (&r0)[row][col]; }
    float operator()(int row, int col) const { return (&r0)[row][col]; }

    // Унарный минус
    float4x4 operator-() const { return float4x4(-r0, -r1, -r2, -r3); }

    // Покомпонентное сложение/вычитание
    float4x4 operator+(const float4x4& other) const { return float4x4(r0 + other.r0, r1 + other.r1, r2 + other.r2, r3 + other.r3); }
    float4x4 operator-(const float4x4& other) const { return float4x4(r0 - other.r0, r1 - other.r1, r2 - other.r2, r3 - other.r3); }
    float4x4& operator+=(const float4x4& other) { r0 += other.r0; r1 += other.r1; r2 += other.r2; r3 += other.r3; return *this; }
    float4x4& operator-=(const float4x4& other) { r0 -= other.r0; r1 -= other.r1; r2 -= other.r2; r3 -= other.r3; return *this; }

    // Умножение/деление на скаляр
    float4x4 operator*(float s) const { return float4x4(r0 * s, r1 * s, r2 * s, r3 * s); }
    float4x4 operator/(float s) const { return float4x4(r0 / s, r1 / s, r2 / s, r3 / s); }
    float4x4& operator*=(float s) { r0 *= s; r1 *= s; r2 *= s; r3 *= s; return *this; }
    float4x4& operator/=(float s) { r0 /= s; r1 /= s; r2 /= s; r3 /= s; return *this; }

    // Сравнение
    bool operator==(const float4x4& other) const { return r0 == other.r0 && r1 == other.r1 && r2 == other.r2 && r3 == other.r3; }
    bool operator!=(const float4x4& other) const { return !(*this == other); }
};

// Скалярные операции слева для матрицы
inline float4x4 operator*(float s, const float4x4& m) { return m * s; }
inline float4x4 operator/(float s, const float4x4& m) { return float4x4(s / m.r0, s / m.r1, s / m.r2, s / m.r3); }

// Умножение матрицы на столбец (матрица * вектор-столбец)
inline float4 operator*(const float4x4& m, const float4& v) {
    return float4(dot(m.r0, v), dot(m.r1, v), dot(m.r2, v), dot(m.r3, v));
}

// Умножение строки на матрицу (вектор-строка * матрица)
inline float4 operator*(const float4& v, const float4x4& m) {
    return m.r0 * v.x + m.r1 * v.y + m.r2 * v.z + m.r3 * v.w;
}

// Умножение матриц (row-major * row-major)
inline float4x4 operator*(const float4x4& a, const float4x4& b) {
    return float4x4(a.r0 * b, a.r1 * b, a.r2 * b, a.r3 * b);
}

// Транспонирование
inline float4x4 transpose(const float4x4& m) {
    __m128 row0 = m.r0.v;
    __m128 row1 = m.r1.v;
    __m128 row2 = m.r2.v;
    __m128 row3 = m.r3.v;

    __m128 tmp0 = _mm_shuffle_ps(row0, row1, _MM_SHUFFLE(1, 0, 1, 0));
    __m128 tmp2 = _mm_shuffle_ps(row0, row1, _MM_SHUFFLE(3, 2, 3, 2));
    __m128 tmp1 = _mm_shuffle_ps(row2, row3, _MM_SHUFFLE(1, 0, 1, 0));
    __m128 tmp3 = _mm_shuffle_ps(row2, row3, _MM_SHUFFLE(3, 2, 3, 2));

    row0 = _mm_shuffle_ps(tmp0, tmp1, _MM_SHUFFLE(2, 0, 2, 0));
    row1 = _mm_shuffle_ps(tmp0, tmp1, _MM_SHUFFLE(3, 1, 3, 1));
    row2 = _mm_shuffle_ps(tmp2, tmp3, _MM_SHUFFLE(2, 0, 2, 0));
    row3 = _mm_shuffle_ps(tmp2, tmp3, _MM_SHUFFLE(3, 1, 3, 1));

    return float4x4(float4(row0), float4(row1), float4(row2), float4(row3));
}

// Определитель (детерминант)
inline float determinant(const float4x4& m) {
    const float4& r0 = m.r0;
    const float4& r1 = m.r1;
    const float4& r2 = m.r2;
    const float4& r3 = m.r3;

    float minor00 = r1.y * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.y * r3.w - r2.w * r3.y) + r1.w * (r2.y * r3.z - r2.z * r3.y);
    float minor01 = r1.x * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.z - r2.z * r3.x);
    float minor02 = r1.x * (r2.y * r3.w - r2.w * r3.y) - r1.y * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.y - r2.y * r3.x);
    float minor03 = r1.x * (r2.y * r3.z - r2.z * r3.y) - r1.y * (r2.x * r3.z - r2.z * r3.x) + r1.z * (r2.x * r3.y - r2.y * r3.x);

    return r0.x * minor00 - r0.y * minor01 + r0.z * minor02 - r0.w * minor03;
}

// Обратная матрица (методом алгебраических дополнений)
inline float4x4 inverse(const float4x4& m) {
    float det = determinant(m);
    if (std::abs(det) < 1e-12f) return float4x4(); // близка к вырожденной, возвращаем identity

    float invDet = 1.0f / det;

    const float4& r0 = m.r0;
    const float4& r1 = m.r1;
    const float4& r2 = m.r2;
    const float4& r3 = m.r3;

    float4 res0, res1, res2, res3;

    // Первая строка алгебраических дополнений (кофакторы первого столбца)
    res0.x = (r1.y * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.y * r3.w - r2.w * r3.y) + r1.w * (r2.y * r3.z - r2.z * r3.y));
    res0.y = -(r1.x * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.z - r2.z * r3.x));
    res0.z = (r1.x * (r2.y * r3.w - r2.w * r3.y) - r1.y * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.y - r2.y * r3.x));
    res0.w = -(r1.x * (r2.y * r3.z - r2.z * r3.y) - r1.y * (r2.x * r3.z - r2.z * r3.x) + r1.z * (r2.x * r3.y - r2.y * r3.x));

    // Вторая строка (кофакторы второго столбца)
    res1.x = -(r0.y * (r2.z * r3.w - r2.w * r3.z) - r0.z * (r2.y * r3.w - r2.w * r3.y) + r0.w * (r2.y * r3.z - r2.z * r3.y));
    res1.y = (r0.x * (r2.z * r3.w - r2.w * r3.z) - r0.z * (r2.x * r3.w - r2.w * r3.x) + r0.w * (r2.x * r3.z - r2.z * r3.x));
    res1.z = -(r0.x * (r2.y * r3.w - r2.w * r3.y) - r0.y * (r2.x * r3.w - r2.w * r3.x) + r0.w * (r2.x * r3.y - r2.y * r3.x));
    res1.w = (r0.x * (r2.y * r3.z - r2.z * r3.y) - r0.y * (r2.x * r3.z - r2.z * r3.x) + r0.z * (r2.x * r3.y - r2.y * r3.x));

    // Третья строка (кофакторы третьего столбца)
    res2.x = (r0.y * (r1.z * r3.w - r1.w * r3.z) - r0.z * (r1.y * r3.w - r1.w * r3.y) + r0.w * (r1.y * r3.z - r1.z * r3.y));
    res2.y = -(r0.x * (r1.z * r3.w - r1.w * r3.z) - r0.z * (r1.x * r3.w - r1.w * r3.x) + r0.w * (r1.x * r3.z - r1.z * r3.x));
    res2.z = (r0.x * (r1.y * r3.w - r1.w * r3.y) - r0.y * (r1.x * r3.w - r1.w * r3.x) + r0.w * (r1.x * r3.y - r1.y * r3.x));
    res2.w = -(r0.x * (r1.y * r3.z - r1.z * r3.y) - r0.y * (r1.x * r3.z - r1.z * r3.x) + r0.z * (r1.x * r3.y - r1.y * r3.x));

    // Четвертая строка (кофакторы четвертого столбца)
    res3.x = -(r0.y * (r1.z * r2.w - r1.w * r2.z) - r0.z * (r1.y * r2.w - r1.w * r2.y) + r0.w * (r1.y * r2.z - r1.z * r2.y));
    res3.y = (r0.x * (r1.z * r2.w - r1.w * r2.z) - r0.z * (r1.x * r2.w - r1.w * r2.x) + r0.w * (r1.x * r2.z - r1.z * r2.x));
    res3.z = -(r0.x * (r1.y * r2.w - r1.w * r2.y) - r0.y * (r1.x * r2.w - r1.w * r2.x) + r0.w * (r1.x * r2.y - r1.y * r2.x));
    res3.w = (r0.x * (r1.y * r2.z - r1.z * r2.y) - r0.y * (r1.x * r2.z - r1.z * r2.x) + r0.z * (r1.x * r2.y - r1.y * r2.x));

    float4x4 cofactor(res0, res1, res2, res3);
    return transpose(cofactor) * invDet;
}

// Вспомогательные функции для создания матриц
inline float4x4 zero() { return float4x4(0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0); }
inline float4x4 identity() { return float4x4(); }

inline float4x4 translation(float x, float y, float z) {
    return float4x4(1,0,0,x, 0,1,0,y, 0,0,1,z, 0,0,0,1);
}
inline float4x4 translation(const float3& t) { return translation(t.x, t.y, t.z); }

inline float4x4 scaling(float x, float y, float z) {
    return float4x4(x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1);
}
inline float4x4 scaling(const float3& s) { return scaling(s.x, s.y, s.z); }
inline float4x4 scaling(float s) { return scaling(s, s, s); }

inline float4x4 rotationX(float angle) {
    float c = cosf(angle), s = sinf(angle);
    return float4x4(1,0,0,0, 0,c,-s,0, 0,s,c,0, 0,0,0,1);
}
inline float4x4 rotationY(float angle) {
    float c = cosf(angle), s = sinf(angle);
    return float4x4(c,0,s,0, 0,1,0,0, -s,0,c,0, 0,0,0,1);
}
inline float4x4 rotationZ(float angle) {
    float c = cosf(angle), s = sinf(angle);
    return float4x4(c,-s,0,0, s,c,0,0, 0,0,1,0, 0,0,0,1);
}

// Перспективная проекция (левосторонняя, row-major, как в HLSL)
inline float4x4 perspectiveLH(float fovY, float aspect, float zn, float zf) {
    float yScale = 1.0f / tanf(fovY * 0.5f);
    float xScale = yScale / aspect;
    return float4x4(xScale,0,0,0, 0,yScale,0,0, 0,0,zf/(zf-zn),1, 0,0,-zn*zf/(zf-zn),0);
}

// Матрица вида (левосторонняя)
inline float4x4 lookAtLH(const float3& eye, const float3& at, const float3& up) {
    float3 zaxis = normalize(at - eye);
    float3 xaxis = normalize(cross(up, zaxis));
    float3 yaxis = cross(zaxis, xaxis);

    return float4x4(float4(xaxis.x, xaxis.y, xaxis.z, -dot(xaxis, eye)),
                    float4(yaxis.x, yaxis.y, yaxis.z, -dot(yaxis, eye)),
                    float4(zaxis.x, zaxis.y, zaxis.z, -dot(zaxis, eye)),
                    float4(0,0,0,1));
}
