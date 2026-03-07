#include "pch.h"

#include <ppl.h>

#include <SoftX/SoftX.h>
#include <SoftX/ThreadPoolManager.h>
#include "RasterizerCommon.h"

//#define DEBUG_TILING

SOFTX_BEGIN

void DeviceContext::DrawPoint(int x, int y, float z, const float4& color)
{
    IRenderTarget* rt = m_RenderTarget;
    if (!rt) return;

    if (!m_DepthBuffer)
		return;

    if (x < 0 || x >= rt->width() || y < 0 || y >= rt->height())
        return;
    int idx = y * rt->width() + x;
    if (z < m_DepthBuffer->at(idx))
    {
		m_DepthBuffer->at(idx) = z;
        rt->set_pixel(int2(x, y), color);
    }
}

void DeviceContext::DrawLine(int x0, int y0, int x1, int y1, float z0, float z1, const float4& color)
{
    IRenderTarget* rt = m_RenderTarget;
    if (!rt) return;

    if (!m_DepthBuffer)
		return;

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

void DeviceContext::DrawIndexed(uint32_t indexCount, uint32_t startIndex) 
{
	PROFILE_SCOPE("DeviceContext::DrawIndexed");

    if (!m_VertexShader || !m_PixelShader || m_VertexBuffer.IsEmpty() || m_IndexBuffer.IsEmpty() || !m_RenderTarget)
        return;

    IRenderTarget* rt = m_RenderTarget;
    DepthBuffer* db = m_DepthBuffer;
    if (!rt || !db) return;

    int width = rt->width();
    int height = rt->height();

    std::vector<VertexOutput> transformedVerts;
	std::vector<int3> triangles;

    transformedVerts.clear();
	triangles.clear();

    std::vector<uint32_t> uniqueIndices;
    {
        std::vector<bool> visited(m_VertexBuffer.Size(), false);
        for (uint32_t i = startIndex; i < startIndex + indexCount; ++i)
        {
            uint32_t idx = m_IndexBuffer.GetByIndex(i);
            if (!visited[idx])
            {
                visited[idx] = true;
                uniqueIndices.push_back(idx);
            }
        }
    }

    transformedVerts.resize(m_VertexBuffer.Size());

    concurrency::parallel_for_each(uniqueIndices.begin(), uniqueIndices.end(), [&](uint32_t idx) {
        VertexOutput out = m_VertexShader(m_VertexBuffer.GetByIndex(idx), m_ConstantBuffer);
		RasterizerCommon::ClipSpaceToScreenSpace(out, m_Viewport);
		transformedVerts[idx] = out;
    });

    std::vector<int3> sourceTriangles;
    for (uint32_t i = startIndex; i < startIndex + indexCount; i += 3) {
        if (i + 2 >= startIndex + indexCount) break;
        uint32_t i0 = m_IndexBuffer.GetByIndex(i);
        uint32_t i1 = m_IndexBuffer.GetByIndex(i + 1);
        uint32_t i2 = m_IndexBuffer.GetByIndex(i + 2);
        sourceTriangles.push_back({(int)i0, (int)i1, (int)i2});
    }

    if (m_GeometryShader) {
        std::vector<VertexOutput> finalVerts;
        std::vector<int3> finalTriangles;

        for (const auto& tri : sourceTriangles) {
            VertexOutput inVerts[3] = {
                transformedVerts[tri.x],
                transformedVerts[tri.y],
                transformedVerts[tri.z]
            };

            std::vector<VertexOutput> outVerts;
            std::vector<int> outIndices;
            m_GeometryShader(inVerts, outVerts, outIndices);

            int baseIndex = (int)finalVerts.size();
            finalVerts.insert(finalVerts.end(), outVerts.begin(), outVerts.end());

            for (size_t j = 0; j + 2 < outIndices.size(); j += 3) {
                finalTriangles.push_back({
                    baseIndex + outIndices[j],
                    baseIndex + outIndices[j + 1],
                    baseIndex + outIndices[j + 2]
                });
            }
        }

        transformedVerts = std::move(finalVerts);
		triangles = std::move(finalTriangles);
    } else {
		triangles = std::move(sourceTriangles);
    }

    if (m_fillMode == FillMode::Solid) {
		RasterizerState state;
		state.cullMode = m_cullMode;
		state.fillMode = m_fillMode;
		state.depthFunc = m_depthFunc;

		Renderer renderer(*m_Rasterizer, *m_RenderTarget, *m_DepthBuffer, m_PixelShader, m_ConstantBuffer, state, m_TileSize);

		renderer.Execute(transformedVerts, triangles);
#ifdef DEBUG_TILING
		DrawActiveTileBorders(renderer.GetTiles());
#endif
    } else if (m_fillMode == FillMode::Wireframe) {
        float4 wireColor(1,1,1,1);
        for (const auto& tri : triangles) {
			const auto& v0 = transformedVerts[tri.x];
			const auto& v1 = transformedVerts[tri.y];
			const auto& v2 = transformedVerts[tri.z];
            DrawLine((int)round(v0.Position.x), (int)round(v0.Position.y),
                     (int)round(v1.Position.x), (int)round(v1.Position.y),
                     v0.Position.z, v1.Position.z, wireColor);
            DrawLine((int)round(v1.Position.x), (int)round(v1.Position.y),
                     (int)round(v2.Position.x), (int)round(v2.Position.y),
                     v1.Position.z, v2.Position.z, wireColor);
            DrawLine((int)round(v2.Position.x), (int)round(v2.Position.y),
                     (int)round(v0.Position.x), (int)round(v0.Position.y),
                     v2.Position.z, v0.Position.z, wireColor);
        }
    } else if (m_fillMode == FillMode::Point) {
		std::vector<bool> drawn(transformedVerts.size(), false);
        for (const auto& tri : triangles) {
            for (int idx : {tri.x, tri.y, tri.z}) {
                if (!drawn[idx]) {
                    drawn[idx] = true;
					const auto& v = transformedVerts[idx];
                    DrawPoint((int)round(v.Position.x), (int)round(v.Position.y), v.Position.z, v.Color);
                }
            }
        }
    }
}

void DeviceContext::DrawIndexed()
{
	uint32_t count = (uint32_t)m_IndexBuffer.Size();
	DrawIndexed(count, 0);
}

void DeviceContext::renderTileQuad(const Tile& tile, float invW, float invH)
{
	PROFILE_SCOPE("DeviceContext::renderTileQuad");
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;

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

void DeviceContext::DrawFullScreenQuad()
{
	PROFILE_SCOPE("DeviceContext::DrawFullScreenQuad");

	if (!m_PixelShader || !m_RenderTarget)
		return;

	IRenderTarget* rt = m_RenderTarget;
	int w = rt->width();
	int h = rt->height();
	float invW = 1.0f / (w - 1);
	float invH = 1.0f / (h - 1);

	// Строим тайлы локально — TiledRenderer здесь не нужен,
	// renderTileQuad не работает с треугольниками.
	std::vector<Tile> tiles;
	{
		int ts = m_TileSize;
		int tilesX = (w + ts - 1) / ts;
		int tilesY = (h + ts - 1) / ts;
		tiles.reserve(tilesX * tilesY);
		for (int ty = 0; ty < tilesY; ++ty)
			for (int tx = 0; tx < tilesX; ++tx)
			{
				int2 mn(tx * ts, ty * ts);
				int2 mx(std::min((tx + 1) * ts - 1, w - 1), std::min((ty + 1) * ts - 1, h - 1));
				tiles.emplace_back(mn, mx);
			}
	}

	int numTiles = (int)tiles.size();
	std::atomic<int> tileIndex(0);

	auto worker = [this, &tiles, invW, invH, &tileIndex, numTiles]() {
		while (true)
		{
			int idx = tileIndex.fetch_add(1);
			if (idx >= numTiles)
				break;
			renderTileQuad(tiles[idx], invW, invH);
		}
	};

	auto& pool = ThreadPoolManager::Get();
	for (int i = 0; i < (int)pool.threadCount(); ++i)
		pool.enqueue(worker);
	pool.wait();

#ifdef DEBUG_TILING
	DrawActiveTileBorders(tiles);
#endif
}

SOFTX_END
