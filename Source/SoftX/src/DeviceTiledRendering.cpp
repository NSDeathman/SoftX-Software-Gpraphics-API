#include "pch.h"
#include <SoftX/SoftX.h>
#include <atomic>

SOFTX_BEGIN

// ========== Методы для работы с тайлами ==========

void Device::buildTiles(int width, int height)
{
    m_tiles.clear();
    int tileSize = m_DeviceContext.GetTileSize();   // берём размер из контекста
    int tilesX = (width + tileSize - 1) / tileSize;
    int tilesY = (height + tileSize - 1) / tileSize;
    for (int ty = 0; ty < tilesY; ++ty)
    {
        for (int tx = 0; tx < tilesX; ++tx)
        {
            int2 min(tx * tileSize, ty * tileSize);
            int2 max(std::min((tx + 1) * tileSize - 1, width - 1),
                     std::min((ty + 1) * tileSize - 1, height - 1));
            m_tiles.emplace_back(min, max);
        }
    }
}

void Device::binTriangles(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles)
{
    // Очищаем списки треугольников для каждого тайла
    for (auto& tile : m_tiles)
        tile.triangleIndices.clear();

    int tileSize = m_DeviceContext.GetTileSize();
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) return;   // если нет рендертаргета – выходим
    int rtWidth = rt->width();
    int rtHeight = rt->height();

    for (int triIdx = 0; triIdx < (int)triangles.size(); ++triIdx)
    {
        const auto& tri = triangles[triIdx];
        const VertexOutput& v0 = verts[tri.x];
        const VertexOutput& v1 = verts[tri.y];
        const VertexOutput& v2 = verts[tri.z];

        float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
        float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
        float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
        float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

        // Преобразуем в индексы тайлов
        int tileX0 = std::max(0, (int)(minX / tileSize));
        int tileY0 = std::max(0, (int)(minY / tileSize));
        int tileX1 = std::min((int)(maxX / tileSize), (rtWidth - 1) / tileSize);
        int tileY1 = std::min((int)(maxY / tileSize), (rtHeight - 1) / tileSize);

#ifdef DEBUG_TILES
        if (triIdx < 5)
        {
            char buf[256];
            sprintf_s(buf, "Tri %d: bbox=(%.1f,%.1f)-(%.1f,%.1f) tileX=[%d,%d] tileY=[%d,%d]\n",
                      triIdx, minX, minY, maxX, maxY, tileX0, tileX1, tileY0, tileY1);
            OutputDebugStringA(buf);
        }
#endif

        for (int ty = tileY0; ty <= tileY1; ++ty)
        {
            for (int tx = tileX0; tx <= tileX1; ++tx)
            {
                int tileIdx = ty * ((rtWidth + tileSize - 1) / tileSize) + tx;
                if (tileIdx < (int)m_tiles.size())
                {
                    m_tiles[tileIdx].triangleIndices.push_back(triIdx);
                }
            }
        }
    }
}

void Device::renderTilesMultithreaded()
{
    int numTiles = (int)m_tiles.size();
    std::atomic<int> tileIndex(0);

    auto worker = [this, &tileIndex, numTiles]() {
        while (true)
        {
            int idx = tileIndex.fetch_add(1);
            if (idx >= numTiles) break;
            renderTile(idx);
        }
    };

    int numThreads = (int)m_threadPool->threadCount();
    for (int i = 0; i < numThreads; ++i)
    {
        m_threadPool->enqueue(worker);
    }
    m_threadPool->wait();
}

void Device::renderTilesSingleThreaded()
{
    for (size_t i = 0; i < m_tiles.size(); ++i)
    {
        renderTile((int)i);
    }
}

void Device::RasterizeTriangleTile(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax)
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

    // Пересекаем с тайлом
    int iMinX = std::max((int)std::ceil(minX), tileMin.x);
    int iMaxX = std::min((int)std::floor(maxX), tileMax.x);
    int iMinY = std::max((int)std::ceil(minY), tileMin.y);
    int iMaxY = std::min((int)std::floor(maxY), tileMax.y);

    if (iMinX > iMaxX || iMinY > iMaxY)
        return;

    // Площадь треугольника и culling
    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);
    CullMode cull = m_DeviceContext.GetCullMode();
    if (cull == CullMode::Back && area2 < 0) return;
    if (cull == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) return;

    auto ps = m_DeviceContext.GetPixelShader();
    auto cb = m_DeviceContext.GetConstantBuffer();

    // Растеризация (скалярная)
    for (int y = iMinY; y <= iMaxY; ++y)
    {
        for (int x = iMinX; x <= iMaxX; ++x)
        {
            float2 p((float)x + 0.5f, (float)y + 0.5f);

            float f0 = edgeFunction(v1.Position, v2.Position, p);
            float f1 = edgeFunction(v2.Position, v0.Position, p);
            float f2 = edgeFunction(v0.Position, v1.Position, p);

            if ((area2 > 0 && (f0 < 0 || f1 < 0 || f2 < 0)) ||
                (area2 < 0 && (f0 > 0 || f1 > 0 || f2 > 0)))
            {
                continue;
            }

            float a = f0 / area2;
            float b = f1 / area2;
            float c = f2 / area2;

            float z = a * v0.Position.z + b * v1.Position.z + c * v2.Position.z;
            float4 color = a * v0.Color + b * v1.Color + c * v2.Color;
            float2 uv = a * v0.UV + b * v1.UV + c * v2.UV;

            int idx = y * width + x;
            if (z < m_depthBuffer.at(idx))
            {
                m_depthBuffer.at(idx) = z;

                VertexOutput frag;
                frag.Position = float4((float)x, (float)y, z, 1.0f);
                frag.Color = color;
                frag.UV = uv;

                float4 finalColor = ps(frag, cb);
                rt->set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

void Device::RasterizeTriangleTileSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax)
{
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) return;
    int width = rt->width();
    int height = rt->height();

    // Bounding box треугольника
    float triMinX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float triMaxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float triMinY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float triMaxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    // Пересекаем с тайлом
    int iMinX = std::max((int)std::ceil(triMinX), tileMin.x);
    int iMaxX = std::min((int)std::floor(triMaxX), tileMax.x);
    int iMinY = std::max((int)std::ceil(triMinY), tileMin.y);
    int iMaxY = std::min((int)std::floor(triMaxY), tileMax.y);

    if (iMinX > iMaxX || iMinY > iMaxY)
        return;

    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);
    CullMode cull = m_DeviceContext.GetCullMode();
    if (cull == CullMode::Back && area2 < 0) return;
    if (cull == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) return;

    auto ps = m_DeviceContext.GetPixelShader();
    auto cb = m_DeviceContext.GetConstantBuffer();

    // ---------- SSE-часть ----------
    {
        float4 dx01 = v1.Position - v0.Position;
        float4 dx12 = v2.Position - v1.Position;
        float4 dx20 = v0.Position - v2.Position;

        __m128 v0x = _mm_set1_ps(v0.Position.x);
        __m128 v0y = _mm_set1_ps(v0.Position.y);
        __m128 v1x = _mm_set1_ps(v1.Position.x);
        __m128 v1y = _mm_set1_ps(v1.Position.y);
        __m128 v2x = _mm_set1_ps(v2.Position.x);
        __m128 v2y = _mm_set1_ps(v2.Position.y);

        __m128 v0z = _mm_set1_ps(v0.Position.z);
        __m128 v1z = _mm_set1_ps(v1.Position.z);
        __m128 v2z = _mm_set1_ps(v2.Position.z);

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

        __m128 v0u = _mm_set1_ps(v0.UV.x);
        __m128 v0v = _mm_set1_ps(v0.UV.y);
        __m128 v1u = _mm_set1_ps(v1.UV.x);
        __m128 v1v = _mm_set1_ps(v1.UV.y);
        __m128 v2u = _mm_set1_ps(v2.UV.x);
        __m128 v2v = _mm_set1_ps(v2.UV.y);

        __m128 invArea = _mm_set1_ps(1.0f / area2);

        __m128 dx01v = _mm_set1_ps(dx01.x);
        __m128 dy01v = _mm_set1_ps(dx01.y);
        __m128 dx12v = _mm_set1_ps(dx12.x);
        __m128 dy12v = _mm_set1_ps(dx12.y);
        __m128 dx20v = _mm_set1_ps(dx20.x);
        __m128 dy20v = _mm_set1_ps(dx20.y);

        for (int y = iMinY; y <= iMaxY; ++y)
        {
            __m128 baseY = _mm_set1_ps(y + 0.5f);

            int xStart = iMinX;
            int xEnd = iMaxX;

            int xBlockStart = (xStart + 3) & ~3;
            int xBlockEnd = xEnd & ~3;

            // Левый остаток
            for (int x = xStart; x < xBlockStart; ++x)
            {
                float2 p((float)x + 0.5f, (float)y + 0.5f);
                float f0 = edgeFunction(v1.Position, v2.Position, p);
                float f1 = edgeFunction(v2.Position, v0.Position, p);
                float f2 = edgeFunction(v0.Position, v1.Position, p);
                if ((area2 > 0 && (f0 < 0 || f1 < 0 || f2 < 0)) || (area2 < 0 && (f0 > 0 || f1 > 0 || f2 > 0)))
                    continue;

                float a = f0 / area2;
                float b = f1 / area2;
                float c = f2 / area2;
                float z = a * v0.Position.z + b * v1.Position.z + c * v2.Position.z;
                float4 color = a * v0.Color + b * v1.Color + c * v2.Color;
                float2 uv = a * v0.UV + b * v1.UV + c * v2.UV;

                int idx = y * width + x;
                if (z < m_depthBuffer.at(idx))
                {
                    m_depthBuffer.at(idx) = z;
                    VertexOutput frag;
                    frag.Position = float4((float)x, (float)y, z, 1.0f);
                    frag.Color = color;
                    frag.UV = uv;
                    float4 finalColor = ps(frag, cb);
                    rt->set_pixel(int2(x, y), finalColor);
                }
            }

            // SSE-блоки
            for (int x = xBlockStart; x < xBlockEnd; x += 4)
            {
                __m128 baseX = _mm_set_ps(x + 3.5f, x + 2.5f, x + 1.5f, x + 0.5f);

                __m128 f01 = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(baseX, v0x), dy01v), _mm_mul_ps(_mm_sub_ps(baseY, v0y), dx01v));
                __m128 f12 = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(baseX, v1x), dy12v), _mm_mul_ps(_mm_sub_ps(baseY, v1y), dx12v));
                __m128 f20 = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(baseX, v2x), dy20v), _mm_mul_ps(_mm_sub_ps(baseY, v2y), dx20v));

                __m128 zero = _mm_setzero_ps();
                __m128 inside;
                if (area2 > 0)
                    inside = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(f01, zero), _mm_cmpge_ps(f12, zero)), _mm_cmpge_ps(f20, zero));
                else
                    inside = _mm_and_ps(_mm_and_ps(_mm_cmple_ps(f01, zero), _mm_cmple_ps(f12, zero)), _mm_cmple_ps(f20, zero));

                int insideMask = _mm_movemask_ps(inside);
                if (insideMask == 0) continue;

                __m128 alpha = _mm_mul_ps(f12, invArea);
                __m128 beta  = _mm_mul_ps(f20, invArea);
                __m128 gamma = _mm_mul_ps(f01, invArea);

                __m128 z = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0z), _mm_mul_ps(beta, v1z)), _mm_mul_ps(gamma, v2z));
                __m128 r = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cr), _mm_mul_ps(beta, v1cr)), _mm_mul_ps(gamma, v2cr));
                __m128 g = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cg), _mm_mul_ps(beta, v1cg)), _mm_mul_ps(gamma, v2cg));
                __m128 b = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cb), _mm_mul_ps(beta, v1cb)), _mm_mul_ps(gamma, v2cb));
                __m128 a = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0ca), _mm_mul_ps(beta, v1ca)), _mm_mul_ps(gamma, v2ca));
                __m128 u = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0u), _mm_mul_ps(beta, v1u)), _mm_mul_ps(gamma, v2u));
                __m128 v = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0v), _mm_mul_ps(beta, v1v)), _mm_mul_ps(gamma, v2v));

                int idx0 = y * width + x;
                __m128 depths = _mm_loadu_ps(&m_depthBuffer.at(idx0));
                __m128 depthCmp = _mm_cmplt_ps(z, depths);
                int depthMask = _mm_movemask_ps(depthCmp) & insideMask;
                if (depthMask == 0) continue;

                float zArr[4], rArr[4], gArr[4], bArr[4], aArr[4], uArr[4], vArr[4];
                _mm_storeu_ps(zArr, z);
                _mm_storeu_ps(rArr, r);
                _mm_storeu_ps(gArr, g);
                _mm_storeu_ps(bArr, b);
                _mm_storeu_ps(aArr, a);
                _mm_storeu_ps(uArr, u);
                _mm_storeu_ps(vArr, v);

                for (int i = 0; i < 4; ++i)
                {
                    if (depthMask & (1 << i))
                    {
                        int px = x + i;
                        int py = y;
                        int idx = py * width + px;
                        m_depthBuffer.at(idx) = zArr[i];

                        VertexOutput frag;
                        frag.Position = float4((float)px, (float)py, zArr[i], 1.0f);
                        frag.Color = float4(rArr[i], gArr[i], bArr[i], aArr[i]);
                        frag.UV = float2(uArr[i], vArr[i]);

                        float4 finalColor = ps(frag, cb);
                        rt->set_pixel(int2(px, py), finalColor);
                    }
                }
            }

            // Правый остаток
            for (int x = xBlockEnd; x <= xEnd; ++x)
            {
                float2 p((float)x + 0.5f, (float)y + 0.5f);
                float f0 = edgeFunction(v1.Position, v2.Position, p);
                float f1 = edgeFunction(v2.Position, v0.Position, p);
                float f2 = edgeFunction(v0.Position, v1.Position, p);
                if ((area2 > 0 && (f0 < 0 || f1 < 0 || f2 < 0)) || (area2 < 0 && (f0 > 0 || f1 > 0 || f2 > 0)))
                    continue;

                float a = f0 / area2;
                float b = f1 / area2;
                float c = f2 / area2;
                float z = a * v0.Position.z + b * v1.Position.z + c * v2.Position.z;
                float4 color = a * v0.Color + b * v1.Color + c * v2.Color;
                float2 uv = a * v0.UV + b * v1.UV + c * v2.UV;

                int idx = y * width + x;
                if (z < m_depthBuffer.at(idx))
                {
                    m_depthBuffer.at(idx) = z;
                    VertexOutput frag;
                    frag.Position = float4((float)x, (float)y, z, 1.0f);
                    frag.Color = color;
                    frag.UV = uv;
                    float4 finalColor = ps(frag, cb);
                    rt->set_pixel(int2(x, y), finalColor);
                }
            }
        }
    }
}

void Device::renderTile(int tileIndex)
{
    const Tile& tile = m_tiles[tileIndex];

#ifdef DEBUG_TILES
    if (!tile.triangleIndices.empty())
    {
        IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
        if (rt)
        {
            for (int x = tile.min.x; x <= tile.max.x; ++x)
            {
                rt->set_pixel(int2(x, tile.min.y), float4(1, 0, 0, 1));
                rt->set_pixel(int2(x, tile.max.y), float4(1, 0, 0, 1));
            }
            for (int y = tile.min.y; y <= tile.max.y; ++y)
            {
                rt->set_pixel(int2(tile.min.x, y), float4(1, 0, 0, 1));
                rt->set_pixel(int2(tile.max.x, y), float4(1, 0, 0, 1));
            }
        }
    }
#endif

    for (int triIdx : tile.triangleIndices)
    {
        const auto& tri = m_triangles[triIdx];
        RasterizeTriangleTileSSE(m_transformedVerts[tri.x], m_transformedVerts[tri.y], m_transformedVerts[tri.z], tile.min, tile.max);
    }
}

SOFTX_END
