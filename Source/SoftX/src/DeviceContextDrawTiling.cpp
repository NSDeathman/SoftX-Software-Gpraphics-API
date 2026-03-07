#include "pch.h"

#include <SoftX/SoftX.h>
#include <SoftX/ThreadPoolManager.h>

#include <atomic>

//#define DEBUG_TILES

SOFTX_BEGIN

void DeviceContext::buildTiles(int width, int height)
{
	PROFILE_SCOPE("DeviceContext::buildTiles");

	m_tiles.clear();
	int tileSize = m_TileSize;
	int tilesX = (width + tileSize - 1) / tileSize;
	int tilesY = (height + tileSize - 1) / tileSize;

	// Инвариант: каждый пиксель принадлежит ровно одному тайлу.
	// Тайл покрывает пиксели [tx*tileSize, (tx+1)*tileSize - 1] по x
	// и [ty*tileSize, (ty+1)*tileSize - 1] по y, clamp'd по границам экрана.
	// Соседние тайлы НЕ пересекаются — это гарантирует thread-safety renderTiles().
	for (int ty = 0; ty < tilesY; ++ty)
	{
		for (int tx = 0; tx < tilesX; ++tx)
		{
			int2 min(tx * tileSize, ty * tileSize);
			int2 max(std::min((tx + 1) * tileSize - 1, width - 1), std::min((ty + 1) * tileSize - 1, height - 1));
			m_tiles.emplace_back(min, max);
		}
	}
}

void DeviceContext::binTriangles(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles)
{
	PROFILE_SCOPE("DeviceContext::binTriangles");

	for (auto& tile : m_tiles)
		tile.triangleIndices.clear();

	int tileSize = m_TileSize;
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;

	int rtWidth = rt->width();
	int rtHeight = rt->height();
	// Должно совпадать с buildTiles — используем те же формулы
	int tilesX = (rtWidth + tileSize - 1) / tileSize;
	int tilesY = (rtHeight + tileSize - 1) / tileSize;

	for (int triIdx = 0; triIdx < (int)triangles.size(); ++triIdx)
	{
		const auto& tri = triangles[triIdx];
		const VertexOutput& v0 = verts[tri.x];
		const VertexOutput& v1 = verts[tri.y];
		const VertexOutput& v2 = verts[tri.z];

		// Без отступа ±0.5f: он был избыточен и создавал data race —
		// треугольник попадал в соседний тайл, и два потока писали
		// в одни пиксели на границе.
		float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
		float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
		float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
		float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

		// Определяем диапазон тайлов которые реально перекрывает треугольник.
		// floor(minX) / tileSize — тайл в котором начинается bbox.
		// ceil(maxX)  / tileSize — тайл в котором заканчивается bbox.
		// Используем int-деление (эквивалент floor для неотрицательных чисел).
		int tileX0 = std::max(0, (int)std::floor(minX) / tileSize);
		int tileY0 = std::max(0, (int)std::floor(minY) / tileSize);
		int tileX1 = std::min(tilesX - 1, (int)std::ceil(maxX) / tileSize);
		int tileY1 = std::min(tilesY - 1, (int)std::ceil(maxY) / tileSize);

		for (int ty = tileY0; ty <= tileY1; ++ty)
			for (int tx = tileX0; tx <= tileX1; ++tx)
				m_tiles[ty * tilesX + tx].triangleIndices.push_back(triIdx);
	}
}

void DeviceContext::renderTile(int tileIndex)
{
	const Tile& tile = m_tiles[tileIndex];

	// Каждый тайл рендерится ровно одним потоком.
	// Растеризатор пишет только в пиксели [tile.min, tile.max] —
	// это не пересекается с другими тайлами по инварианту buildTiles().
	RasterizerState state;
	state.cullMode = m_cullMode;
	state.fillMode = m_fillMode;
	state.depthFunc = m_depthFunc;

	for (int triIdx : tile.triangleIndices)
	{
		const auto& tri = m_triangles[triIdx];
		m_Rasterizer->RasterizeTriangle(m_transformedVerts[tri.x], m_transformedVerts[tri.y], m_transformedVerts[tri.z],
										state, *m_DepthBuffer, *m_RenderTarget, m_PixelShader, m_ConstantBuffer,
										tile.min, tile.max);
	}
}

void DeviceContext::renderTiles()
{
	PROFILE_SCOPE("DeviceContext::renderTilesMultithreaded");

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

void DeviceContext::renderTileQuad(int tileIndex, float invW, float invH)
{
	PROFILE_SCOPE("DeviceContext::renderTileQuad");
	const Tile& tile = m_tiles[tileIndex];
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;

	int w = rt->width();
	VertexOutput input = {};
	auto ps = m_PixelShader;
	auto cb = m_ConstantBuffer;
	for (int y = tile.min.y; y <= tile.max.y; ++y)
	{
		float v = y * invH;
		for (int x = tile.min.x; x <= tile.max.x; ++x)
		{
			float u = x * invW;
			input.UV = float2(u, v);
			float4 color = ps(input, cb);
			rt->set_pixel(int2(x, y), color);
		}
	}
}

SOFTX_END
