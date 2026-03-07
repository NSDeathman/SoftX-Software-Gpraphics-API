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
    DepthBuffer(uint2 size)
        : resolution(size)
        , widthPadded((size.x + 3) & ~3) // округляем до кратного 4
        , blocks((((size.x + 3) & ~3) * size.y) / 4)
    {
        Clear(1.0f);
    }

    // ── Очистка ───────────────────────────────────────────────────────────
    void Clear(float depth)
    {
        __m128 depth4 = _mm_set1_ps(depth);
        __m128* ptr   = blocks.data();
        size_t  count = blocks.size();

        size_t i = 0;
        // Потоковая запись — минует кэш, максимальная скорость
        for (; i < count; ++i)
            _mm_stream_ps(reinterpret_cast<float*>(ptr + i), depth4);
        _mm_sfence();
    }

    // ── Скалярный доступ (обратная совместимость) ─────────────────────────
    float Read(int2 coords) const
    {
        return FloatPtr()[coords.y * widthPadded + coords.x];
    }

    void Write(int2 coords, float depth)
    {
        FloatPtr()[coords.y * widthPadded + coords.x] = depth;
    }

    float& At(int2 coords)
    {
        return FloatPtr()[coords.y * widthPadded + coords.x];
    }
    const float& At(int2 coords) const
    {
        return FloatPtr()[coords.y * widthPadded + coords.x];
    }

    // At(int index) — линейный индекс по оригинальной ширине (как раньше)
    float& At(uint index)
    {
        int x = index % resolution.x;
        int y = index / resolution.x;
        return At(int2(x, y));
    }
    const float& At(uint index) const
    {
        int x = index % resolution.x;
        int y = index / resolution.x;
        return At(int2(x, y));
    }

    // ── SIMD блочный доступ ───────────────────────────────────────────────
    // Читает 4 значения глубины начиная с coords (горизонтально).
    // coords.x должен быть кратен 4 — используется в SSE растеризаторе.
    __m128 Read4(uint2 coords) const
    {
        assert(coords.x % 4 == 0);
        assert(coords.x >= 0 && coords.x + 3u < widthPadded &&
               coords.y >= 0 && coords.y < resolution.y);
        int blockIdx = (coords.y * widthPadded + coords.x) / 4u;
        return blocks[blockIdx];
    }

    // Записывает 4 значения глубины с учётом маски (1 = перезаписать).
    // Используется в SSE растеризаторе после depth test.
    void Write4(uint2 coords, __m128 depths, __m128 mask)
    {
        assert(coords.x % 4 == 0);
        assert(coords.x >= 0 && coords.x + 3 < widthPadded &&
               coords.y >= 0 && coords.y < resolution.y);
        int     blockIdx = (coords.y * widthPadded + coords.x) / 4;
        __m128& block    = blocks[blockIdx];

        // Записываем только те пиксели где маска = 0xFFFFFFFF
        // block = (depths & mask) | (block & ~mask)
        block = _mm_or_ps(_mm_and_ps(depths, mask),
                          _mm_andnot_ps(mask, block));
    }

    // Depth test для 4 пикселей: возвращает маску прошедших (z < buffer).
    // depth4 — интерполированные z, activeMask — маска покрытия тайла.
    __m128 Test4(uint2 coords, __m128 depth4, __m128 activeMask) const
    {
        __m128 buffered = Read4(coords);
        __m128 passed   = _mm_cmplt_ps(depth4, buffered); // z < bufferZ
        return _mm_and_ps(passed, activeMask);
    }

    // ── Геттеры ───────────────────────────────────────────────────────────
    uint Width() const
    {
        return resolution.x;
    }
    uint Height() const
    {
        return resolution.y;
    }
    uint WidthPadded() const
    {
        return widthPadded;
    }
    uint2 Size() const
    {
        return resolution;
    }

    // Сырой доступ к float* — для скалярного растеризатора
    float* Data()
    {
        return FloatPtr();
    }
    const float* data() const
    {
        return FloatPtr();
    }

private:
    float* FloatPtr()
    {
        return reinterpret_cast<float*>(blocks.data());
    }
    const float* FloatPtr() const
    {
        return reinterpret_cast<const float*>(blocks.data());
    }

    uint2 resolution;
    uint widthPadded;                // кратен 4 — для выравнивания блоков
    std::vector<__m128> blocks;     // каждый блок = 4 float глубины
};

SOFTX_END
