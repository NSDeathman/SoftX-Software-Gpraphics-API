#include "pch.h"

#include <SoftX/SoftX.h>

SOFTX_BEGIN

Renderer::Renderer(
    IRasterizer& rasterizer, 
    IRenderTarget& rt, 
    DepthBuffer& db,
    const PixelShader& ps, 
    const ConstantBuffer& cb, 
    const TextureTable* tt,
    const RasterizerState& state, 
    uint32_t tileSize): 
    m_Rasterizer(rasterizer), 
    m_RenderTarget(rt), 
    m_DepthBuffer(db), 
    m_PS(ps), 
    m_CB(cb), 
    m_TT(tt),
    m_State(state), 
    m_TileSize(tileSize)
{
}

void Renderer::Execute(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles)
{
	m_Verts = &verts;
	m_Triangles = &triangles;
	buildTiles(m_RenderTarget.width(), m_RenderTarget.height());
	binTriangles(verts, triangles);
	renderTiles();
	m_Verts = nullptr;
	m_Triangles = nullptr;
}

void Renderer::buildTiles(int width, int height)
{
    m_Tiles.clear();
    int ts = m_TileSize;
    int tilesX = (width  + ts - 1) / ts;
    int tilesY = (height + ts - 1) / ts;
    for (int ty = 0; ty < tilesY; ++ty)
        for (int tx = 0; tx < tilesX; ++tx)
        {
            int2 mn(tx * ts, ty * ts);
            int2 mx(std::min((tx+1)*ts - 1, width - 1),
                    std::min((ty+1)*ts - 1, height - 1));
            m_Tiles.emplace_back(mn, mx);
        }
}

void Renderer::binTriangles(
    const std::vector<VertexOutput>& verts,
    const std::vector<int3>& triangles)
{
    for (auto& t : m_Tiles) t.triangleIndices.clear();

    int ts      = m_TileSize;
    int tilesX  = (m_RenderTarget.width()  + ts - 1) / ts;
    int tilesY  = (m_RenderTarget.height() + ts - 1) / ts;

    for (int triIdx = 0; triIdx < (int)triangles.size(); ++triIdx)
    {
		const auto& tri = triangles[triIdx];
		const auto& v0 = verts[tri.x];
		const auto& v1 = verts[tri.y];
		const auto& v2 = verts[tri.z];

        float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
        float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
        float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
        float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

        int tileX0 = std::max(0,          (int)std::floor(minX) / ts);
        int tileY0 = std::max(0,          (int)std::floor(minY) / ts);
        int tileX1 = std::min(tilesX - 1, (int)std::ceil(maxX)  / ts);
        int tileY1 = std::min(tilesY - 1, (int)std::ceil(maxY)  / ts);

        for (int ty = tileY0; ty <= tileY1; ++ty)
            for (int tx = tileX0; tx <= tileX1; ++tx)
                m_Tiles[ty * tilesX + tx].triangleIndices.push_back(triIdx);
    }
}

void Renderer::renderTile(int tileIndex)
{
    const Tile& tile = m_Tiles[tileIndex];
}

void Renderer::renderTiles()
{
    int numTiles = (int)m_Tiles.size();
    std::atomic<int> tileIndex(0);

    auto worker = [this, &tileIndex, numTiles]() {
        while (true)
        {
            int idx = tileIndex.fetch_add(1);
            if (idx >= numTiles) break;

            const Tile& tile = m_Tiles[idx];
            for (int triIdx : tile.triangleIndices)
            {
                // (*m_Triangles)[triIdx] — разыменовываем указатель на вектор
                const int3& tri = (*m_Triangles)[triIdx];

                m_Rasterizer.RasterizeTriangle(
                    (*m_Verts)[tri.x],   // разыменовываем и m_Verts
                    (*m_Verts)[tri.y],
                    (*m_Verts)[tri.z],
                    m_State,
                    m_DepthBuffer,
                    m_RenderTarget,
                    m_PS,
                    m_CB,
                    m_TT,
                    tile.min,
                    tile.max);
            }
        }
    };

    auto& pool = ThreadPoolManager::Get();
    for (int i = 0; i < (int)pool.threadCount(); ++i)
        pool.enqueue(worker);
    pool.wait();
}


SOFTX_END
