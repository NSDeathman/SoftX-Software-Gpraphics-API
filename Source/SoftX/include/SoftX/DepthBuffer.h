#pragma once
#include <xmmintrin.h>
#include <vector>
#include <algorithm>
#include <limits>
#include <cassert>
#include "ThirdPartyIncluding.h"
#include "LibInternal.h"

SOFTX_BEGIN

class SOFTX_API DepthBuffer
{
public:
    DepthBuffer(int2 size)
        : m_width(size.x)
        , m_height(size.y)
        , m_widthPadded((size.x + 3) & ~3) // округляем до кратного 4
        , m_blocks((((size.x + 3) & ~3) * size.y) / 4)
    {
        clear(1.0f);
    }

    // ── Очистка ───────────────────────────────────────────────────────────
    void clear(float depth)
    {
        __m128 depth4 = _mm_set1_ps(depth);
        __m128* ptr   = m_blocks.data();
        size_t  count = m_blocks.size();

        size_t i = 0;
        // Потоковая запись — минует кэш, максимальная скорость
        for (; i < count; ++i)
            _mm_stream_ps(reinterpret_cast<float*>(ptr + i), depth4);
        _mm_sfence();
    }

    // ── Скалярный доступ (обратная совместимость) ─────────────────────────
    float read(int2 coords) const
    {
        if (coords.x < 0 || coords.x >= m_width ||
            coords.y < 0 || coords.y >= m_height)
            return 1.0f;
        return floatPtr()[coords.y * m_widthPadded + coords.x];
    }

    void write(int2 coords, float depth)
    {
        if (coords.x < 0 || coords.x >= m_width ||
            coords.y < 0 || coords.y >= m_height)
            return;
        floatPtr()[coords.y * m_widthPadded + coords.x] = depth;
    }

    float& at(int2 coords)
    {
        assert(coords.x >= 0 && coords.x < m_width &&
               coords.y >= 0 && coords.y < m_height);
        return floatPtr()[coords.y * m_widthPadded + coords.x];
    }
    const float& at(int2 coords) const
    {
        assert(coords.x >= 0 && coords.x < m_width &&
               coords.y >= 0 && coords.y < m_height);
        return floatPtr()[coords.y * m_widthPadded + coords.x];
    }

    // at(int index) — линейный индекс по оригинальной ширине (как раньше)
    float& at(int index)
    {
        int x = index % m_width;
        int y = index / m_width;
        return at(int2(x, y));
    }
    const float& at(int index) const
    {
        int x = index % m_width;
        int y = index / m_width;
        return at(int2(x, y));
    }

    // ── SIMD блочный доступ ───────────────────────────────────────────────
    // Читает 4 значения глубины начиная с coords (горизонтально).
    // coords.x должен быть кратен 4 — используется в SSE растеризаторе.
    __m128 read4(int2 coords) const
    {
        assert(coords.x % 4 == 0);
        assert(coords.x >= 0 && coords.x + 3 < m_widthPadded &&
               coords.y >= 0 && coords.y < m_height);
        int blockIdx = (coords.y * m_widthPadded + coords.x) / 4;
        return m_blocks[blockIdx];
    }

    // Записывает 4 значения глубины с учётом маски (1 = перезаписать).
    // Используется в SSE растеризаторе после depth test.
    void write4(int2 coords, __m128 depths, __m128 mask)
    {
        assert(coords.x % 4 == 0);
        assert(coords.x >= 0 && coords.x + 3 < m_widthPadded &&
               coords.y >= 0 && coords.y < m_height);
        int     blockIdx = (coords.y * m_widthPadded + coords.x) / 4;
        __m128& block    = m_blocks[blockIdx];

        // Записываем только те пиксели где маска = 0xFFFFFFFF
        // block = (depths & mask) | (block & ~mask)
        block = _mm_or_ps(_mm_and_ps(depths, mask),
                          _mm_andnot_ps(mask, block));
    }

    // Depth test для 4 пикселей: возвращает маску прошедших (z < buffer).
    // depth4 — интерполированные z, activeMask — маска покрытия тайла.
    __m128 test4(int2 coords, __m128 depth4, __m128 activeMask) const
    {
        __m128 buffered = read4(coords);
        __m128 passed   = _mm_cmplt_ps(depth4, buffered); // z < bufferZ
        return _mm_and_ps(passed, activeMask);
    }

    // ── Геттеры ───────────────────────────────────────────────────────────
    int  width()       const { return m_width; }
    int  height()      const { return m_height; }
    int  widthPadded() const { return m_widthPadded; }
    int2 size()        const { return int2(m_width, m_height); }

    // Сырой доступ к float* — для скалярного растеризатора
    float*       data()       { return floatPtr(); }
    const float* data() const { return floatPtr(); }

private:
    float* floatPtr()
    {
        return reinterpret_cast<float*>(m_blocks.data());
    }
    const float* floatPtr() const
    {
        return reinterpret_cast<const float*>(m_blocks.data());
    }

    int m_width;
    int m_height;
    int m_widthPadded;                // кратен 4 — для выравнивания блоков
    std::vector<__m128> m_blocks;     // каждый блок = 4 float глубины
};

SOFTX_END
