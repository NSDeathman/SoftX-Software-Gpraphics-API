#include "pch.h"
#include <SoftX/SoftX.h>

SOFTX_BEGIN

void Device::DrawPoint(int x, int y, float z, const float4& color)
{
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) return;
    if (x < 0 || x >= rt->width() || y < 0 || y >= rt->height())
        return;
    int idx = y * rt->width() + x;
    if (z < m_depthBuffer.at(idx))
    {
        m_depthBuffer.at(idx) = z;
        rt->set_pixel(int2(x, y), color);
    }
}

void Device::DrawLine(int x0, int y0, int x1, int y1, float z0, float z1, const float4& color)
{
    // Целочисленный алгоритм Брезенхема с интерполяцией глубины
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int steps = std::max(dx, -dy);
    float zStep = (steps > 0) ? (z1 - z0) / steps : 0.0f;
    float z = z0;
    int x = x0, y = y0;
    for (int i = 0; i <= steps; ++i)
    {
        DrawPoint(x, y, z, color);
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            y += sy;
        }
        z += zStep;
    }
}

float4 Device::ClipToScreen(const float4& clipPos) const
{
    Viewport vp = m_DeviceContext.GetViewport(); // используем контекст

    // Извлекаем компоненты с помощью SSE
    __m128 pos = clipPos.v;
    float w = clipPos.w;
    float invW = 1.0f / w;

    // Вектор ndc
    __m128 ndc = _mm_mul_ps(pos, _mm_set1_ps(invW)); // x/w, y/w, z/w, 1

    // Извлекаем x,y,z
    float xNDC = _mm_cvtss_f32(ndc);
    float yNDC = _mm_cvtss_f32(_mm_shuffle_ps(ndc, ndc, 1));
    float zNDC = _mm_cvtss_f32(_mm_shuffle_ps(ndc, ndc, 2));

    // Скалярные вычисления
    float screenX = vp.pos.x + (xNDC * 0.5f + 0.5f) * vp.size.x;
    float screenY = vp.pos.y + (1.0f - (yNDC * 0.5f + 0.5f)) * vp.size.y;
    float screenZ = vp.minZ + zNDC * (vp.maxZ - vp.minZ);

    return float4(screenX, screenY, screenZ, 1.0f);
}

VertexOutput Interpolate(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float a, float b, float c)
{
    VertexOutput result;

    // Интерполяция позиции
    result.Position = a * v0.Position + b * v1.Position + c * v2.Position;

    // Интерполяция цвета
    result.Color = a * v0.Color + b * v1.Color + c * v2.Color;

    // Интерполяция текстурных координат
    result.UV = a * v0.UV + b * v1.UV + c * v2.UV;

    return result;
}

void Device::RasterizeTriangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2)
{
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) return;
    int width = rt->width();
    int height = rt->height();

    // 1. Вычисляем bounding box треугольника
    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    // Отсекаем по границам экрана
    int iMinX = std::max(0, (int)std::floor(minX));
    int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
    int iMinY = std::max(0, (int)std::floor(minY));
    int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

    // 2. Предвычисляем площадь треугольника (удвоенная)
    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);

    CullMode cull = m_DeviceContext.GetCullMode();
    if (cull == CullMode::Back && area2 < 0)
        return;
    if (cull == CullMode::Front && area2 > 0)
        return;

    if (std::abs(area2) < 1e-6f)
        return; // вырожденный треугольник

    auto ps = m_DeviceContext.GetPixelShader();
    auto cb = m_DeviceContext.GetConstantBuffer();

    // 3. Проходим по всем пикселям bounding box
    for (int y = iMinY; y <= iMaxY; ++y)
    {
        for (int x = iMinX; x <= iMaxX; ++x)
        {
            // Центр пикселя (с половинным смещением)
            float2 p((float)x + 0.5f, (float)y + 0.5f);

            // Вычисляем барицентрические координаты через edge-функции
            float f0 = edgeFunction(v1.Position, v2.Position, p);
            float f1 = edgeFunction(v2.Position, v0.Position, p);
            float f2 = edgeFunction(v0.Position, v1.Position, p);

            if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0)
                continue;

            // Нормируем на полную площадь
            float a = f0 / area2;
            float b = f1 / area2;
            float c = f2 / area2;

            // Интерполяция атрибутов
            float z = a * v0.Position.z + b * v1.Position.z + c * v2.Position.z;
            float4 color = a * v0.Color + b * v1.Color + c * v2.Color;
            float2 uv = a * v0.UV + b * v1.UV + c * v2.UV;

            // Проверка глубины
            int idx = y * width + x;
            if (z < m_depthBuffer.at(idx))
            {
                m_depthBuffer.at(idx) = z;

                // Формируем VertexOutput для пиксельного шейдера
                VertexOutput frag;
                frag.Position = float4((float)x, (float)y, z, 1.0f);
                frag.Color = color;
                frag.UV = uv;

                // Вызов пиксельного шейдера
                float4 finalColor = ps(frag, cb);

                // Запись во фреймбуфер
                rt->set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

void Device::RasterizeTriangleSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2)
{
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) return;
    int width = rt->width();
    int height = rt->height();

    // Bounding box треугольника
    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    int iMinX = std::max(0, (int)std::floor(minX));
    int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
    int iMinY = std::max(0, (int)std::floor(minY));
    int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

    // Площадь треугольника (удвоенная)
    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);

    CullMode cull = m_DeviceContext.GetCullMode();
    if (cull == CullMode::Back && area2 < 0)
        return;
    if (cull == CullMode::Front && area2 > 0)
        return;
    if (std::abs(area2) < 1e-6f)
        return;

    auto ps = m_DeviceContext.GetPixelShader();
    auto cb = m_DeviceContext.GetConstantBuffer();

    // Предвычисляем константы для edge-функций
    float4 dx01_ = v1.Position - v0.Position;
    float4 dx12_ = v2.Position - v1.Position;
    float4 dx20_ = v0.Position - v2.Position;

    // Для удобства сохраняем атрибуты вершин в векторы
    __m128 v0x = _mm_set1_ps(v0.Position.x);
    __m128 v0y = _mm_set1_ps(v0.Position.y);
    __m128 v1x = _mm_set1_ps(v1.Position.x);
    __m128 v1y = _mm_set1_ps(v1.Position.y);
    __m128 v2x = _mm_set1_ps(v2.Position.x);
    __m128 v2y = _mm_set1_ps(v2.Position.y);

    __m128 v0z = _mm_set1_ps(v0.Position.z);
    __m128 v1z = _mm_set1_ps(v1.Position.z);
    __m128 v2z = _mm_set1_ps(v2.Position.z);

    // Цвета (компоненты)
    __m128 v0cr = _mm_set1_ps(v0.Color.x);
    __m128 v0cg = _mm_set1_ps(v0.Color.y);
    __m128 v0cb = _mm_set1_ps(v0.Color.z);
    __m128 v0ca = _mm_set1_ps(v0.Color.w);

    __m128 v1cr = _mm_set1_ps(v1.Color.x);
    __m128 v1cg = _mm_set1_ps(v1.Color.y);
    __m128 v1cb = _mm_set1_ps(v1.Color.z);
    __m128 v1ca = _mm_set1_ps(v1.Color.w);

    __m128 v2cr = _mm_set1_ps(v2.Color.x);
    __m128 v2cg = _mm_set1_ps(v2.Color.y);
    __m128 v2cb = _mm_set1_ps(v2.Color.z);
    __m128 v2ca = _mm_set1_ps(v2.Color.w);

    // UV
    __m128 v0u = _mm_set1_ps(v0.UV.x);
    __m128 v0v = _mm_set1_ps(v0.UV.y);
    __m128 v1u = _mm_set1_ps(v1.UV.x);
    __m128 v1v = _mm_set1_ps(v1.UV.y);
    __m128 v2u = _mm_set1_ps(v2.UV.x);
    __m128 v2v = _mm_set1_ps(v2.UV.y);

    // Инвертированная площадь (для деления)
    __m128 invArea = _mm_set1_ps(1.0f / area2);

    // Константы для edge-функций
    __m128 dx01v = _mm_set1_ps(dx01_.x);
    __m128 dy01v = _mm_set1_ps(dx01_.y);
    __m128 dx12v = _mm_set1_ps(dx12_.x);
    __m128 dy12v = _mm_set1_ps(dx12_.y);
    __m128 dx20v = _mm_set1_ps(dx20_.x);
    __m128 dy20v = _mm_set1_ps(dx20_.y);

    // Цикл по строкам
    for (int y = iMinY; y <= iMaxY; ++y)
    {
        // Базовое значение y для всех пикселей в строке (центр)
        __m128 baseY = _mm_set1_ps(y + 0.5f);

        // Цикл по горизонтальным блокам по 4 пикселя
        for (int x = iMinX; x <= iMaxX; x += 4)
        {
            // Если вышли за правую границу, корректируем размер блока
            int blockWidth = std::min(4, iMaxX - x + 1);
            if (blockWidth < 4)
            {
                // Для последнего неполного блока можно обработать скалярно, но для простоты пропустим
                // В реальном коде лучше сделать отдельный скалярный доводчик
                continue;
            }

            // Координаты центров 4 пикселей: x+0.5, x+1+0.5, x+2+0.5, x+3+0.5
            __m128 baseX = _mm_set_ps(x + 3.5f, x + 2.5f, x + 1.5f, x + 0.5f);

            // Вычисляем edge-функции
            __m128 f01 =
                _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(baseX, v0x), dy01v), _mm_mul_ps(_mm_sub_ps(baseY, v0y), dx01v));
            __m128 f12 =
                _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(baseX, v1x), dy12v), _mm_mul_ps(_mm_sub_ps(baseY, v1y), dx12v));
            __m128 f20 =
                _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(baseX, v2x), dy20v), _mm_mul_ps(_mm_sub_ps(baseY, v2y), dx20v));

            // Проверка принадлежности треугольнику (с учётом знака площади)
            __m128 zero = _mm_setzero_ps();
            __m128 maskInside;
            if (area2 > 0)
            {
                maskInside =
                    _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(f01, zero), _mm_cmpge_ps(f12, zero)), _mm_cmpge_ps(f20, zero));
            }
            else
            {
                maskInside =
                    _mm_and_ps(_mm_and_ps(_mm_cmple_ps(f01, zero), _mm_cmple_ps(f12, zero)), _mm_cmple_ps(f20, zero));
            }
            int insideMask = _mm_movemask_ps(maskInside);
            if (insideMask == 0)
                continue;

            // Барицентрические координаты (alpha для v1,v2, beta для v2,v0, gamma для v0,v1)
            __m128 alpha = _mm_mul_ps(f12, invArea); // напротив v0? зависит от определения, но сумма должна быть 1
            __m128 beta = _mm_mul_ps(f20, invArea);
            __m128 gamma = _mm_mul_ps(f01, invArea);

            // Интерполяция глубины
            __m128 z = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0z), _mm_mul_ps(beta, v1z)), _mm_mul_ps(gamma, v2z));

            // Интерполяция цвета (компоненты)
            __m128 r = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cr), _mm_mul_ps(beta, v1cr)), _mm_mul_ps(gamma, v2cr));
            __m128 g = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cg), _mm_mul_ps(beta, v1cg)), _mm_mul_ps(gamma, v2cg));
            __m128 b = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cb), _mm_mul_ps(beta, v1cb)), _mm_mul_ps(gamma, v2cb));
            __m128 a = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0ca), _mm_mul_ps(beta, v1ca)), _mm_mul_ps(gamma, v2ca));

            // Интерполяция UV
            __m128 u = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0u), _mm_mul_ps(beta, v1u)), _mm_mul_ps(gamma, v2u));
            __m128 v = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0v), _mm_mul_ps(beta, v1v)), _mm_mul_ps(gamma, v2v));

            // Загрузка текущих глубин из буфера (невыровненная загрузка)
            int idx0 = y * width + x;
            __m128 depths = _mm_loadu_ps(&m_depthBuffer.at(idx0)); // предполагаем, что at возвращает ссылку

            // Сравнение глубин (z < depths)
            __m128 depthCmp = _mm_cmplt_ps(z, depths);
            int depthMask = _mm_movemask_ps(depthCmp) & insideMask;
            if (depthMask == 0)
                continue;

            // Для каждого пикселя в маске вызываем пиксельный шейдер
            // Распаковываем результаты в массивы
            float zArr[4], rArr[4], gArr[4], bArr[4], aArr[4], uArr[4], vArr[4];
            _mm_storeu_ps(zArr, z);
            _mm_storeu_ps(rArr, r);
            _mm_storeu_ps(gArr, g);
            _mm_storeu_ps(bArr, b);
            _mm_storeu_ps(aArr, a);
            _mm_storeu_ps(uArr, u);
            _mm_storeu_ps(vArr, v);

            // Проходим по 4 пикселям
            for (int i = 0; i < 4; ++i)
            {
                int bit = 1 << i;
                if (depthMask & bit)
                {
                    // Индекс пикселя
                    int px = x + i;
                    int py = y;
                    int idx = py * width + px;

                    // Записываем новую глубину
                    m_depthBuffer.at(idx) = zArr[i];

                    // Формируем VertexOutput для шейдера
                    VertexOutput frag;
                    frag.Position = float4((float)px, (float)py, zArr[i], 1.0f);
                    frag.Color = float4(rArr[i], gArr[i], bArr[i], aArr[i]);
                    frag.UV = float2(uArr[i], vArr[i]);

                    // Вызов пиксельного шейдера
                    float4 finalColor = ps(frag, cb);

                    // Запись во фреймбуфер
                    rt->set_pixel(int2(px, py), finalColor);
                }
            }
        }
    }
}

SOFTX_END
