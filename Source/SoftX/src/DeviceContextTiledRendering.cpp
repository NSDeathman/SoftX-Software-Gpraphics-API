#include "pch.h"

#include <SoftX/SoftX.h>
#include <SoftX/ThreadPoolManager.h>

#include <atomic>

//#define DEBUG_TILES

SOFTX_BEGIN

// ========== Методы для работы с тайлами ==========

void DeviceContext::buildTiles(int width, int height)
{
    m_tiles.clear();
	int tileSize = m_TileSize;
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

void DeviceContext::binTriangles(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles)
{
    // Очищаем списки треугольников для каждого тайла
    for (auto& tile : m_tiles)
        tile.triangleIndices.clear();

    int tileSize = m_TileSize;
    IRenderTarget* rt = m_RenderTarget;
    if (!rt) return;   // если нет рендертаргета – выходим
    int rtWidth = rt->width();
    int rtHeight = rt->height();

    for (int triIdx = 0; triIdx < (int)triangles.size(); ++triIdx)
    {
        const auto& tri = triangles[triIdx];
        const VertexOutput& v0 = verts[tri.x];
        const VertexOutput& v1 = verts[tri.y];
        const VertexOutput& v2 = verts[tri.z];

        float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x}) - 0.5f;
		float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x}) + 0.5f;
		float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y}) - 0.5f;
		float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y}) + 0.5f;

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

void DeviceContext::renderTilesMultithreaded()
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

    auto& pool = ThreadPoolManager::Get();
	int numThreads = (int)pool.threadCount();
    for (int i = 0; i < numThreads; ++i)
    {
		pool.enqueue(worker);
    }
	pool.wait();
}

void DeviceContext::renderTilesSingleThreaded()
{
    for (size_t i = 0; i < m_tiles.size(); ++i)
    {
        renderTile((int)i);
    }
}

void DeviceContext::RasterizeTriangleTile(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax)
{
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;
	if (!m_DepthBuffer)
		return;

	int width = rt->width();
	int height = rt->height();

	// Вычисляем полный bounding box треугольника (как в обычной растеризации)
	float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
	float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
	float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
	float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

	int iMinX = std::max(0, (int)std::floor(minX));
	int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
	int iMinY = std::max(0, (int)std::floor(minY));
	int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

	// Площадь треугольника и culling
	float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);
	CullMode cull = m_cullMode;
	if (cull == CullMode::Back && area2 < 0)
		return;
	if (cull == CullMode::Front && area2 > 0)
		return;
	if (std::abs(area2) < 1e-6f)
		return;

	auto ps = m_PixelShader;
	auto cb = m_ConstantBuffer;

	// Проходим по всем пикселям bounding box
	for (int y = iMinY; y <= iMaxY; ++y)
	{
		for (int x = iMinX; x <= iMaxX; ++x)
		{
			// Проверяем, принадлежит ли пиксель данному тайлу
			if (x < tileMin.x || x > tileMax.x || y < tileMin.y || y > tileMax.y)
				continue;

			float2 p((float)x + 0.5f, (float)y + 0.5f);

			float f0 = edgeFunction(v1.Position, v2.Position, p);
			float f1 = edgeFunction(v2.Position, v0.Position, p);
			float f2 = edgeFunction(v0.Position, v1.Position, p);

			if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0)
				continue;

			float a = f0 / area2;
			float b = f1 / area2;
			float c = f2 / area2;

            VertexOutput frag = trilerp(v0, v1, v2, a, b, c);

			int idx = y * width + x;
			if (frag.Position.z < m_DepthBuffer->at(idx))
			{
				m_DepthBuffer->at(idx) = frag.Position.z;

				frag.Position = float4(frag.Position.xyz(), 1.0f);

				float4 finalColor = ps(frag, cb);
				rt->set_pixel(int2(x, y), finalColor);
			}
		}
	}
}

void DeviceContext::RasterizeTriangleTileSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax)
{
    IRenderTarget* rt = m_RenderTarget;
    if (!rt) return;
    if (!m_DepthBuffer) return;

    int width = rt->width();
    int height = rt->height();

    // Полный bounding box треугольника
    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    int iMinX = std::max(0, (int)std::floor(minX));
    int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
    int iMinY = std::max(0, (int)std::floor(minY));
    int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

    // Площадь и culling
    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);
    if (m_cullMode == CullMode::Back && area2 < 0) return;
    if (m_cullMode == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) return;

    auto ps = m_PixelShader;
    auto cb = m_ConstantBuffer;

    // Предвычисляем константы для edge-функций (как в RasterizeTriangleSSE)
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

        // Обрабатываем строку блоками по 4 пикселя
        int x;
        for (x = iMinX; x <= iMaxX - 3; x += 4)
        {
            // Проверяем, пересекается ли блок с тайлом
            if (x > tileMax.x || x + 3 < tileMin.x)
                continue;

            __m128 baseX = _mm_set_ps(x + 3.5f, x + 2.5f, x + 1.5f, x + 0.5f);

            // Edge-функции
            __m128 f01 = _mm_sub_ps(
                _mm_mul_ps(_mm_sub_ps(baseX, v0x), dy01v),
                _mm_mul_ps(_mm_sub_ps(baseY, v0y), dx01v));
            __m128 f12 = _mm_sub_ps(
                _mm_mul_ps(_mm_sub_ps(baseX, v1x), dy12v),
                _mm_mul_ps(_mm_sub_ps(baseY, v1y), dx12v));
            __m128 f20 = _mm_sub_ps(
                _mm_mul_ps(_mm_sub_ps(baseX, v2x), dy20v),
                _mm_mul_ps(_mm_sub_ps(baseY, v2y), dx20v));

            // Маска принадлежности треугольнику с учётом знака площади
            __m128 zero = _mm_setzero_ps();
            __m128 inside;
            if (area2 > 0)
            {
                inside = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(f01, zero), _mm_cmpge_ps(f12, zero)),
                                    _mm_cmpge_ps(f20, zero));
            }
            else
            {
                inside = _mm_and_ps(_mm_and_ps(_mm_cmple_ps(f01, zero), _mm_cmple_ps(f12, zero)),
                                    _mm_cmple_ps(f20, zero));
            }
            int insideMask = _mm_movemask_ps(inside);
            if (insideMask == 0) continue;

            // Барицентрические координаты
            __m128 alpha = _mm_mul_ps(f12, invArea);
            __m128 beta  = _mm_mul_ps(f20, invArea);
            __m128 gamma = _mm_mul_ps(f01, invArea);

            // Интерполяция
            __m128 z = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0z), _mm_mul_ps(beta, v1z)), _mm_mul_ps(gamma, v2z));
            __m128 r = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cr), _mm_mul_ps(beta, v1cr)), _mm_mul_ps(gamma, v2cr));
            __m128 g = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cg), _mm_mul_ps(beta, v1cg)), _mm_mul_ps(gamma, v2cg));
            __m128 b = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0cb), _mm_mul_ps(beta, v1cb)), _mm_mul_ps(gamma, v2cb));
            __m128 a = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0ca), _mm_mul_ps(beta, v1ca)), _mm_mul_ps(gamma, v2ca));
            __m128 u = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0u), _mm_mul_ps(beta, v1u)), _mm_mul_ps(gamma, v2u));
            __m128 v = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0v), _mm_mul_ps(beta, v1v)), _mm_mul_ps(gamma, v2v));

            // Загрузка глубины
            int idx0 = y * width + x;
            __m128 depths = _mm_loadu_ps(&m_DepthBuffer->at(idx0));

            __m128 depthCmp = _mm_cmplt_ps(z, depths);
            int depthMask = _mm_movemask_ps(depthCmp) & insideMask;
            if (depthMask == 0) continue;

            // Распаковка
            float aArr[4], bArr[4], cArr[4];
			_mm_storeu_ps(aArr, alpha);
			_mm_storeu_ps(bArr, beta);
			_mm_storeu_ps(cArr, gamma);

			for (int i = 0; i < 4; ++i)
			{
				int px = x + i;
				if (px < tileMin.x || px > tileMax.x)
					continue;

				int bit = 1 << i;
				if (depthMask & bit)
				{
					int py = y;
					int idx = py * width + px;

					// Интерполяция всех атрибутов через trilerp
					VertexOutput frag = trilerp(v0, v1, v2, aArr[i], bArr[i], cArr[i]);
					frag.Position = float4((float)px, (float)py, frag.Position.z, 1.0f);
					m_DepthBuffer->at(idx) = frag.Position.z;

					float4 finalColor = ps(frag, cb);
					rt->set_pixel(int2(px, py), finalColor);
				}
			}
        }

        // Скалярный доводчик для оставшихся пикселей
		for (; x <= iMaxX; ++x)
		{
			// Проверка на тайл
			if (x < tileMin.x || x > tileMax.x)
				continue;

			float2 p((float)x + 0.5f, (float)y + 0.5f);

			float f0 = edgeFunction(v1.Position, v2.Position, p);
			float f1 = edgeFunction(v2.Position, v0.Position, p);
			float f2 = edgeFunction(v0.Position, v1.Position, p);

			if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0)
				continue;

			float a = f0 / area2;
			float b = f1 / area2;
			float c = f2 / area2;

			// Интерполяция всех атрибутов через trilerp
			VertexOutput frag = trilerp(v0, v1, v2, a, b, c);

			int idx = y * width + x;
			if (frag.Position.z < m_DepthBuffer->at(idx))
			{
				m_DepthBuffer->at(idx) = frag.Position.z;

				float4 finalColor = ps(frag, cb);
				rt->set_pixel(int2(x, y), finalColor);
			}
		}
    }
}

void DeviceContext::renderTile(int tileIndex)
{
    const Tile& tile = m_tiles[tileIndex];

#ifdef DEBUG_TILES
    if (!tile.triangleIndices.empty())
    {
        IRenderTarget* rt = m_RenderTarget;
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

void DeviceContext::renderTileQuad(int tileIndex)
{
	const Tile& tile = m_tiles[tileIndex];
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;

#ifdef DEBUG_TILES
	if (!tile.triangleIndices.empty())
	{
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

	int w = rt->width();
	int h = rt->height();
	VertexOutput input = {};
	auto ps = m_PixelShader;
	auto cb = m_ConstantBuffer;
	for (int y = tile.min.y; y <= tile.max.y; ++y)
	{
		float v = (float)y / (h - 1);
		for (int x = tile.min.x; x <= tile.max.x; ++x)
		{
			float u = (float)x / (w - 1);
			input.UV = float2(u, v);
			float4 color = ps(input, cb);
			rt->set_pixel(int2(x, y), color);
		}
	}
}


SOFTX_END
