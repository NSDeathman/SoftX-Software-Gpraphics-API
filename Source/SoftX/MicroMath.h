#pragma once

#include <immintrin.h>   // SSE4.2
#include <cmath>         // для fabs в проверках (не обязательно)
#include <algorithm>

#undef min
#undef max

constexpr float PI = 3.14159265358979323846f;
constexpr float DEG_TO_RAD = PI / 180.0f;
constexpr float RAD_TO_DEG = 180.0f / PI;

inline float DegToRad(float degrees) {
    return degrees * DEG_TO_RAD;
}

inline float RadToDeg(float radians) {
    return radians * RAD_TO_DEG;
}

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

    // Операторы сравнения (с плавающей точкой — осторожно, обычно используют эпсилон, но для простоты)
    bool operator==(const float2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const float2& other) const { return !(*this == other); }
};

// Скалярные операции слева
inline float2 operator+(float s, const float2& v) { return v + s; }
inline float2 operator-(float s, const float2& v) { return float2(s - v.x, s - v.y); }
inline float2 operator*(float s, const float2& v) { return v * s; }
inline float2 operator/(float s, const float2& v) { return float2(s / v.x, s / v.y); }

// Алгебраические функции для float2
inline float dot(const float2& a, const float2& b) {
    return a.x * b.x + a.y * b.y;
}

// Длина вектора
inline float length(const float2& v) {
    return std::sqrt(dot(v, v));
}

// Нормализация
inline float2 normalize(const float2& v) {
    float len = length(v);
    if (len > 1e-6f)
        return v * (1.0f / len);
    return float2(0, 0);
}

// Покомпонентный минимум/максимум
inline float2 min(const float2& a, const float2& b) {
    return float2(std::min(a.x, b.x), std::min(a.y, b.y));
}
inline float2 max(const float2& a, const float2& b) {
    return float2(std::max(a.x, b.x), std::max(a.y, b.y));
}

// Абсолютная величина
inline float2 abs(const float2& v) {
    return float2(std::abs(v.x), std::abs(v.y));
}

// Псевдо-векторное произведение (скалярное, площадь параллелограмма)
inline float cross(const float2& a, const float2& b) {
    return a.x * b.y - a.y * b.x;
}

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

    // Операторы сравнения (с плавающей точкой — осторожно)
    bool operator==(const float3& other) const { return x == other.x && y == other.y && z == other.z; }
    bool operator!=(const float3& other) const { return !(*this == other); }
};

// Скалярные операции слева
inline float3 operator+(float s, const float3& v) { return v + s; }
inline float3 operator-(float s, const float3& v) { return float3(s - v.x, s - v.y, s - v.z); }
inline float3 operator*(float s, const float3& v) { return v * s; }
inline float3 operator/(float s, const float3& v) { return float3(s / v.x, s / v.y, s / v.z); }

// Алгебраические функции для float3
inline float dot(const float3& a, const float3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Векторное произведение
inline float3 cross(const float3& a, const float3& b) {
    return float3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

// Длина вектора
inline float length(const float3& v) {
    return std::sqrt(dot(v, v));
}

// Нормализация
inline float3 normalize(const float3& v) {
    float len = length(v);
    if (len > 1e-6f)
        return v * (1.0f / len);
    return float3(0, 0, 0);
}

// Покомпонентный минимум/максимум
inline float3 min(const float3& a, const float3& b) {
    return float3(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z));
}
inline float3 max(const float3& a, const float3& b) {
    return float3(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
}

// Абсолютная величина
inline float3 abs(const float3& v) {
    return float3(std::abs(v.x), std::abs(v.y), std::abs(v.z));
}

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
    int2 operator/(const int2& other) const { return int2(x / other.x, y / other.y); } // целочисленное деление
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

    // Сравнения
    bool operator==(const int2& other) const { return x == other.x && y == other.y; }
    bool operator!=(const int2& other) const { return !(*this == other); }
    bool operator<(const int2& other) const { return x < other.x&& y < other.y; } // покомпонентно
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

// Дополнительные функции для int2 (по аналогии с float2, но без length и normalize)
inline int2 min(const int2& a, const int2& b) {
    return int2(std::min(a.x, b.x), std::min(a.y, b.y));
}
inline int2 max(const int2& a, const int2& b) {
    return int2(std::max(a.x, b.x), std::max(a.y, b.y));
}
inline int2 abs(const int2& v) {
    return int2(std::abs(v.x), std::abs(v.y));
}
inline int dot(const int2& a, const int2& b) {
    return a.x * b.x + a.y * b.y;
}
inline int cross(const int2& a, const int2& b) {
    return a.x * b.y - a.y * b.x;
}

class alignas(16) float4 {
public:
    // Анонимный union для доступа к компонентам и SSE-регистру
    union {
        __m128 v;            // SSE-вектор
        struct { float x, y, z, w; };
    };

    // Конструкторы
    float4() : v(_mm_setzero_ps()) {}
    explicit float4(__m128 vec) : v(vec) {}
    float4(float x, float y, float z, float w) : v(_mm_set_ps(w, z, y, x)) {
        // _mm_set_ps принимает аргументы в обратном порядке: w, z, y, x
    }
    float4(const float4& other) : v(other.v) {}

    // Присваивание
    float4& operator=(const float4& other) {
        v = other.v;
        return *this;
    }
    float4& operator=(__m128 vec) {
        v = vec;
        return *this;
    }

    // Доступ по индексу
    float& operator[](int index) {
        return (&x)[index];
    }
    const float& operator[](int index) const {
        return (&x)[index];
    }

    // Унарный минус
    float4 operator-() const {
        __m128 zero = _mm_setzero_ps();
        return float4(_mm_sub_ps(zero, v));
    }

    // Арифметические операторы (покомпонентно)
    float4 operator+(const float4& other) const {
        return float4(_mm_add_ps(v, other.v));
    }
    float4 operator-(const float4& other) const {
        return float4(_mm_sub_ps(v, other.v));
    }
    float4 operator*(const float4& other) const {
        return float4(_mm_mul_ps(v, other.v));
    }
    float4 operator/(const float4& other) const {
        return float4(_mm_div_ps(v, other.v));
    }

    // Составные присваивания
    float4& operator+=(const float4& other) {
        v = _mm_add_ps(v, other.v);
        return *this;
    }
    float4& operator-=(const float4& other) {
        v = _mm_sub_ps(v, other.v);
        return *this;
    }
    float4& operator*=(const float4& other) {
        v = _mm_mul_ps(v, other.v);
        return *this;
    }
    float4& operator/=(const float4& other) {
        v = _mm_div_ps(v, other.v);
        return *this;
    }

    // Операторы сравнения (возвращают маску в int, но для bool удобно)
    bool operator==(const float4& other) const {
        __m128 cmp = _mm_cmpeq_ps(v, other.v);
        return _mm_movemask_ps(cmp) == 0xF;  // все 4 бита установлены
    }
    bool operator!=(const float4& other) const {
        return !(*this == other);
    }

    // Скалярные операции (для удобства)
    float4 operator+(float s) const {
        return float4(_mm_add_ps(v, _mm_set1_ps(s)));
    }
    float4 operator-(float s) const {
        return float4(_mm_sub_ps(v, _mm_set1_ps(s)));
    }
    float4 operator*(float s) const {
        return float4(_mm_mul_ps(v, _mm_set1_ps(s)));
    }
    float4 operator/(float s) const {
        return float4(_mm_div_ps(v, _mm_set1_ps(s)));
    }

    // Дружественные функции для дополнительных операций
    friend float4 operator+(float s, const float4& v) {
        return v + s;
    }
    friend float4 operator-(float s, const float4& v) {
        return float4(_mm_sub_ps(_mm_set1_ps(s), v.v));
    }
    friend float4 operator*(float s, const float4& v) {
        return v * s;
    }
    friend float4 operator/(float s, const float4& v) {
        return float4(_mm_div_ps(_mm_set1_ps(s), v.v));
    }

    // Составные скалярные операции
    float4& operator*=(float s) {
        v = _mm_mul_ps(v, _mm_set1_ps(s));
        return *this;
    }
    float4& operator/=(float s) {
        v = _mm_div_ps(v, _mm_set1_ps(s));
        return *this;
    }
    float4& operator+=(float s) {
        v = _mm_add_ps(v, _mm_set1_ps(s));
        return *this;
    }
    float4& operator-=(float s) {
        v = _mm_sub_ps(v, _mm_set1_ps(s));
        return *this;
    }
};

// Математические функции (алгебраические)
inline float dot(const float4& a, const float4& b) {
    __m128 mul = _mm_mul_ps(a.v, b.v);
    // Горизонтальная сумма: складываем пары, затем результат
    __m128 shuf = _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 sums = _mm_add_ps(mul, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ps(sums, shuf);
    // Теперь сумма во всех компонентах, берём первую
    float result;
    _mm_store_ss(&result, sums);
    return result;
}

inline float4 cross(const float4& a, const float4& b) {
    // Векторное произведение для первых трёх компонент, w = 0
    // cross(a, b) = (a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x, 0)
    __m128 a_yzx = _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 0, 2, 1)); // из (x,y,z,w) -> (y,z,x,w)
    __m128 b_yzx = _mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 a_zxy = _mm_shuffle_ps(a.v, a.v, _MM_SHUFFLE(3, 1, 0, 2)); // (z,x,y,w)
    __m128 b_zxy = _mm_shuffle_ps(b.v, b.v, _MM_SHUFFLE(3, 1, 0, 2));

    __m128 mul1 = _mm_mul_ps(a_yzx, b_zxy);
    __m128 mul2 = _mm_mul_ps(a_zxy, b_yzx);
    __m128 result = _mm_sub_ps(mul1, mul2);
    // Обнуляем w (можно просто заменить последнюю компоненту на 0)
    result = _mm_blend_ps(result, _mm_setzero_ps(), 0b1000); // сохраняем x,y,z, w=0 (маска 1000 = 8)
    return float4(result);
}

// Модуль (длина)
inline float length(const float4& v) {
    return std::sqrt(dot(v, v));
}

// Нормализация
inline float4 normalize(const float4& v) {
    float len = length(v);
    if (len > 1e-6f) {
        return v * (1.0f / len);
    }
    return float4(0, 0, 0, 0);
}

// Покомпонентный минимум/максимум
inline float4 min(const float4& a, const float4& b) {
    return float4(_mm_min_ps(a.v, b.v));
}
inline float4 max(const float4& a, const float4& b) {
    return float4(_mm_max_ps(a.v, b.v));
}

// Абсолютная величина
inline float4 abs(const float4& v) {
    return float4(_mm_andnot_ps(_mm_set1_ps(-0.0f), v.v)); // убираем знаковый бит
}

// ==================== float4x4 ====================
// Row-major matrix 4x4 (compatible with HLSL default)
class alignas(16) float4x4 {
public:
    // Rows as float4 (using SSE)
    float4 r0, r1, r2, r3;

    // Constructors
    float4x4() : r0(1, 0, 0, 0), r1(0, 1, 0, 0), r2(0, 0, 1, 0), r3(0, 0, 0, 1) {} // identity

    explicit float4x4(const float4& row0, const float4& row1, const float4& row2, const float4& row3)
        : r0(row0), r1(row1), r2(row2), r3(row3) {}

    // Construct from 16 scalars in row-major order (m00, m01, m02, m03, m10, ...)
    float4x4(float m00, float m01, float m02, float m03,
        float m10, float m11, float m12, float m13,
        float m20, float m21, float m22, float m23,
        float m30, float m31, float m32, float m33)
        : r0(m00, m01, m02, m03)
        , r1(m10, m11, m12, m13)
        , r2(m20, m21, m22, m23)
        , r3(m30, m31, m32, m33) {}

    // Copy constructor
    float4x4(const float4x4&) = default;

    // Assignment
    float4x4& operator=(const float4x4&) = default;

    // Access rows
    float4& operator[](int row) { return (&r0)[row]; }
    const float4& operator[](int row) const { return (&r0)[row]; }

    // Access elements (row, col)
    float& operator()(int row, int col) { return (&r0)[row][col]; }
    float operator()(int row, int col) const { return (&r0)[row][col]; }

    // Unary minus
    float4x4 operator-() const {
        return float4x4(-r0, -r1, -r2, -r3);
    }

    // Matrix addition/subtraction (component-wise)
    float4x4 operator+(const float4x4& other) const {
        return float4x4(r0 + other.r0, r1 + other.r1, r2 + other.r2, r3 + other.r3);
    }
    float4x4 operator-(const float4x4& other) const {
        return float4x4(r0 - other.r0, r1 - other.r1, r2 - other.r2, r3 - other.r3);
    }

    float4x4& operator+=(const float4x4& other) {
        r0 += other.r0; r1 += other.r1; r2 += other.r2; r3 += other.r3;
        return *this;
    }
    float4x4& operator-=(const float4x4& other) {
        r0 -= other.r0; r1 -= other.r1; r2 -= other.r2; r3 -= other.r3;
        return *this;
    }

    // Scalar multiplication/division
    float4x4 operator*(float s) const {
        return float4x4(r0 * s, r1 * s, r2 * s, r3 * s);
    }
    float4x4 operator/(float s) const {
        return float4x4(r0 / s, r1 / s, r2 / s, r3 / s);
    }

    float4x4& operator*=(float s) {
        r0 *= s; r1 *= s; r2 *= s; r3 *= s;
        return *this;
    }
    float4x4& operator/=(float s) {
        r0 /= s; r1 /= s; r2 /= s; r3 /= s;
        return *this;
    }

    // Comparison (exact, be careful with floats)
    bool operator==(const float4x4& other) const {
        return r0 == other.r0 && r1 == other.r1 && r2 == other.r2 && r3 == other.r3;
    }
    bool operator!=(const float4x4& other) const {
        return !(*this == other);
    }
};

// Scalar multiplication from left
inline float4x4 operator*(float s, const float4x4& m) {
    return m * s;
}
inline float4x4 operator/(float s, const float4x4& m) {
    return float4x4(s / m.r0, s / m.r1, s / m.r2, s / m.r3);
}

inline float4 operator*(const float4& v, const float4x4& m);

// Matrix multiplication (row-major * row-major)
inline float4x4 operator*(const float4x4& a, const float4x4& b) {
    // Compute each row of result as row_a * matrix_b
    return float4x4(
        a.r0 * b,   // uses float4 * float4x4 (row-vector times matrix)
        a.r1 * b,
        a.r2 * b,
        a.r3 * b
    );
}

// Matrix * column vector (vector is treated as column)
// Result components are dot products of matrix rows with vector
inline float4 operator*(const float4x4& m, const float4& v) {
    return float4(
        dot(m.r0, v),
        dot(m.r1, v),
        dot(m.r2, v),
        dot(m.r3, v)
    );
}

// Row vector * matrix (vector is treated as row)
// Result is linear combination of matrix rows weighted by vector components
inline float4 operator*(const float4& v, const float4x4& m) {
    // v.x * row0 + v.y * row1 + v.z * row2 + v.w * row3
    return m.r0 * v.x + m.r1 * v.y + m.r2 * v.z + m.r3 * v.w;
}

// Transpose
inline float4x4 transpose(const float4x4& m) {
    // Using SSE shuffles for efficiency
    __m128 row0 = m.r0.v;
    __m128 row1 = m.r1.v;
    __m128 row2 = m.r2.v;
    __m128 row3 = m.r3.v;

    // _MM_TRANSPOSE4_PS macro does exactly this
    __m128 tmp0, tmp1, tmp2, tmp3;
    tmp0 = _mm_shuffle_ps(row0, row1, _MM_SHUFFLE(1, 0, 1, 0)); // (row0.low, row1.low)
    tmp2 = _mm_shuffle_ps(row0, row1, _MM_SHUFFLE(3, 2, 3, 2)); // (row0.high, row1.high)
    tmp1 = _mm_shuffle_ps(row2, row3, _MM_SHUFFLE(1, 0, 1, 0)); // (row2.low, row3.low)
    tmp3 = _mm_shuffle_ps(row2, row3, _MM_SHUFFLE(3, 2, 3, 2)); // (row2.high, row3.high)

    row0 = _mm_shuffle_ps(tmp0, tmp1, _MM_SHUFFLE(2, 0, 2, 0)); // col0
    row1 = _mm_shuffle_ps(tmp0, tmp1, _MM_SHUFFLE(3, 1, 3, 1)); // col1
    row2 = _mm_shuffle_ps(tmp2, tmp3, _MM_SHUFFLE(2, 0, 2, 0)); // col2
    row3 = _mm_shuffle_ps(tmp2, tmp3, _MM_SHUFFLE(3, 1, 3, 1)); // col3

    return float4x4(
        float4(row0),
        float4(row1),
        float4(row2),
        float4(row3)
    );
}

// Determinant (optional, but useful)
inline float determinant(const float4x4& m) {
    // Using cofactor expansion (simple but not optimal)
    // For SSE optimized version, one could use _mm_dp_ps etc., but keep it readable for now.
    const float4& r0 = m.r0;
    const float4& r1 = m.r1;
    const float4& r2 = m.r2;
    const float4& r3 = m.r3;

    // Compute minors for first row
    float minor00 = r1.y * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.y * r3.w - r2.w * r3.y) + r1.w * (r2.y * r3.z - r2.z * r3.y);
    float minor01 = r1.x * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.z - r2.z * r3.x);
    float minor02 = r1.x * (r2.y * r3.w - r2.w * r3.y) - r1.y * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.y - r2.y * r3.x);
    float minor03 = r1.x * (r2.y * r3.z - r2.z * r3.y) - r1.y * (r2.x * r3.z - r2.z * r3.x) + r1.z * (r2.x * r3.y - r2.y * r3.x);

    return r0.x * minor00 - r0.y * minor01 + r0.z * minor02 - r0.w * minor03;
}

// Inverse (optional, might be heavy)
inline float4x4 inverse(const float4x4& m) {
    // Using cofactor method. Not optimized but works.
    float det = determinant(m);
    if (std::abs(det) < 1e-12f) return float4x4(); // return identity? or zero? better zero? Let's return zero matrix.
    float invDet = 1.0f / det;

    const float4& r0 = m.r0;
    const float4& r1 = m.r1;
    const float4& r2 = m.r2;
    const float4& r3 = m.r3;

    // Precompute commonly used products
    float4 res0, res1, res2, res3;

    // Compute adjugate matrix (transpose of cofactor matrix)
    // Cofactor for element (i,j) = (-1)^(i+j) * det(minor of element)
    // We'll compute directly the adjugate rows (which become columns after transpose)

    // First row of adjugate (cofactors of first column)
    res0.x = (r1.y * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.y * r3.w - r2.w * r3.y) + r1.w * (r2.y * r3.z - r2.z * r3.y));
    res0.y = -(r1.x * (r2.z * r3.w - r2.w * r3.z) - r1.z * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.z - r2.z * r3.x));
    res0.z = (r1.x * (r2.y * r3.w - r2.w * r3.y) - r1.y * (r2.x * r3.w - r2.w * r3.x) + r1.w * (r2.x * r3.y - r2.y * r3.x));
    res0.w = -(r1.x * (r2.y * r3.z - r2.z * r3.y) - r1.y * (r2.x * r3.z - r2.z * r3.x) + r1.z * (r2.x * r3.y - r2.y * r3.x));

    // Second row of adjugate (cofactors of second column)
    res1.x = -(r0.y * (r2.z * r3.w - r2.w * r3.z) - r0.z * (r2.y * r3.w - r2.w * r3.y) + r0.w * (r2.y * r3.z - r2.z * r3.y));
    res1.y = (r0.x * (r2.z * r3.w - r2.w * r3.z) - r0.z * (r2.x * r3.w - r2.w * r3.x) + r0.w * (r2.x * r3.z - r2.z * r3.x));
    res1.z = -(r0.x * (r2.y * r3.w - r2.w * r3.y) - r0.y * (r2.x * r3.w - r2.w * r3.x) + r0.w * (r2.x * r3.y - r2.y * r3.x));
    res1.w = (r0.x * (r2.y * r3.z - r2.z * r3.y) - r0.y * (r2.x * r3.z - r2.z * r3.x) + r0.z * (r2.x * r3.y - r2.y * r3.x));

    // Third row of adjugate (cofactors of third column)
    res2.x = (r0.y * (r1.z * r3.w - r1.w * r3.z) - r0.z * (r1.y * r3.w - r1.w * r3.y) + r0.w * (r1.y * r3.z - r1.z * r3.y));
    res2.y = -(r0.x * (r1.z * r3.w - r1.w * r3.z) - r0.z * (r1.x * r3.w - r1.w * r3.x) + r0.w * (r1.x * r3.z - r1.z * r3.x));
    res2.z = (r0.x * (r1.y * r3.w - r1.w * r3.y) - r0.y * (r1.x * r3.w - r1.w * r3.x) + r0.w * (r1.x * r3.y - r1.y * r3.x));
    res2.w = -(r0.x * (r1.y * r3.z - r1.z * r3.y) - r0.y * (r1.x * r3.z - r1.z * r3.x) + r0.z * (r1.x * r3.y - r1.y * r3.x));

    // Fourth row of adjugate (cofactors of fourth column)
    res3.x = -(r0.y * (r1.z * r2.w - r1.w * r2.z) - r0.z * (r1.y * r2.w - r1.w * r2.y) + r0.w * (r1.y * r2.z - r1.z * r2.y));
    res3.y = (r0.x * (r1.z * r2.w - r1.w * r2.z) - r0.z * (r1.x * r2.w - r1.w * r2.x) + r0.w * (r1.x * r2.z - r1.z * r2.x));
    res3.z = -(r0.x * (r1.y * r2.w - r1.w * r2.y) - r0.y * (r1.x * r2.w - r1.w * r2.x) + r0.w * (r1.x * r2.y - r1.y * r2.x));
    res3.w = (r0.x * (r1.y * r2.z - r1.z * r2.y) - r0.y * (r1.x * r2.z - r1.z * r2.x) + r0.z * (r1.x * r2.y - r1.y * r2.x));

    // Multiply by invDet and return transposed (because we computed cofactor matrix, need adjugate = transpose of cofactor)
    // But our res0..res3 are rows of cofactor matrix? Actually we computed cofactors for each column, so res0..res3 are rows of the cofactor matrix.
    // To get the inverse, we need to transpose the cofactor matrix and multiply by invDet.
    // So we construct a matrix from res0..res3 (as rows) and then transpose.
    float4x4 cofactor(res0, res1, res2, res3);
    return transpose(cofactor) * invDet;
}

// Additional utility functions
inline float4x4 zero() {
    return float4x4(
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    );
}

inline float4x4 identity() {
    return float4x4();
}

// Build a translation matrix (row-major)
inline float4x4 translation(float x, float y, float z) {
    return float4x4(
        1, 0, 0, x,
        0, 1, 0, y,
        0, 0, 1, z,
        0, 0, 0, 1
    );
}
inline float4x4 translation(const float3& t) {
    return translation(t.x, t.y, t.z);
}

// Build a scaling matrix
inline float4x4 scaling(float x, float y, float z) {
    return float4x4(
        x, 0, 0, 0,
        0, y, 0, 0,
        0, 0, z, 0,
        0, 0, 0, 1
    );
}
inline float4x4 scaling(const float3& s) {
    return scaling(s.x, s.y, s.z);
}
inline float4x4 scaling(float s) {
    return scaling(s, s, s);
}

// Build rotation around X axis (angle in radians)
inline float4x4 rotationX(float angle) {
    float c = cosf(angle), s = sinf(angle);
    return float4x4(
        1, 0, 0, 0,
        0, c, -s, 0,
        0, s, c, 0,
        0, 0, 0, 1
    );
}

// Build rotation around Y axis
inline float4x4 rotationY(float angle) {
    float c = cosf(angle), s = sinf(angle);
    return float4x4(
        c, 0, s, 0,
        0, 1, 0, 0,
        -s, 0, c, 0,
        0, 0, 0, 1
    );
}

// Build rotation around Z axis
inline float4x4 rotationZ(float angle) {
    float c = cosf(angle), s = sinf(angle);
    return float4x4(
        c, -s, 0, 0,
        s, c, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    );
}

// Perspective projection matrix (left-handed, row-major, like HLSL)
inline float4x4 perspectiveLH(float fovY, float aspect, float zn, float zf) {
    float yScale = 1.0f / tanf(fovY * 0.5f);
    float xScale = yScale / aspect;
    return float4x4(
        xScale, 0, 0, 0,
        0, yScale, 0, 0,
        0, 0, zf / (zf - zn), 1,
        0, 0, -zn * zf / (zf - zn), 0
    );
}

inline float4x4 lookAtLH(const float3& eye, const float3& at, const float3& up) {
    float3 zaxis = normalize(at - eye);         // направление взгляда (от глаза к цели)
    float3 xaxis = normalize(cross(up, zaxis)); // право
    float3 yaxis = cross(zaxis, xaxis);         // верх

    // Строки матрицы
    return float4x4(
        float4(xaxis.x, xaxis.y, xaxis.z, -dot(xaxis, eye)),
        float4(yaxis.x, yaxis.y, yaxis.z, -dot(yaxis, eye)),
        float4(zaxis.x, zaxis.y, zaxis.z, -dot(zaxis, eye)),
        float4(0, 0, 0, 1)
    );
}
