#include "pch.h"

#include <SoftX/SoftX.h>

SOFTX_BEGIN

Renderer::Renderer(IRasterizer& rasterizer,
                   IRenderTarget& rt,
                   DepthBuffer& db,
                   const PixelShader& ps,
                   const ConstantBuffer& cb,
                   const TextureTable* tt,
                   const RasterizerState& state,
                   uint tileSize) : 
                   rasterizer(rasterizer),
                   renderTarget(rt),
                   depthBuffer(db),
                   pixelShader(ps),
                   constantBuffer(cb),
                   textureTable(tt),
                   state(state),
                   tileSize(tileSize)
{
}

void Renderer::Execute(const std::vector<VertexOutput>& inputVerts, const std::vector<int3>& inputTriangles)
{
    this->verts = &inputVerts;
    this->triangles = &inputTriangles;
    BuildTiles(renderTarget.Width(), renderTarget.Height());
    BinTriangles(inputVerts, inputTriangles);
    RenderTiles();
    this->verts = nullptr;
    this->triangles = nullptr;
}

void Renderer::BuildTiles(uint width, uint height)
{
    tiles.clear();
    uint ts = tileSize;
    uint tilesX = (width + ts - 1) / ts;
    uint tilesY = (height + ts - 1) / ts;
    for (uint ty = 0; ty < tilesY; ++ty)
    {
        for (uint tx = 0; tx < tilesX; ++tx)
        {
            uint2 mn(tx * ts, ty * ts);
            uint2 mx(std::min((tx + 1) * ts - 1, width - 1),
                     std::min((ty + 1) * ts - 1, height - 1));
            tiles.emplace_back(mn, mx);
        }
    }
}

void Renderer::BinTriangles(const std::vector<VertexOutput>& inputVerts, const std::vector<int3>& inputTriangles)
{
    for (auto& t : tiles)
        t.triangleIndices.clear();

    uint ts = tileSize;
    uint tilesX = (renderTarget.Width() + ts - 1) / ts;
    uint tilesY = (renderTarget.Height() + ts - 1) / ts;

    for (int triIdx = 0; triIdx < static_cast<int>(inputTriangles.size()); ++triIdx)
    {
        const auto& tri = inputTriangles[triIdx];
        const auto& v0 = inputVerts[tri.x];
        const auto& v1 = inputVerts[tri.y];
        const auto& v2 = inputVerts[tri.z];

        float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
        float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
        float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
        float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

        uint tileX0 = uint(std::max(0.0f, std::floor(minX))) / ts;
        uint tileY0 = uint(std::max(0.0f, std::floor(minY))) / ts;
        uint tileX1 = std::min(tilesX - 1, uint(std::ceil(maxX)) / ts);
        uint tileY1 = std::min(tilesY - 1, uint(std::ceil(maxY)) / ts);

        for (uint ty = tileY0; ty <= tileY1; ++ty)
        {
            for (uint tx = tileX0; tx <= tileX1; ++tx)
            {
                tiles[ty * tilesX + tx].triangleIndices.push_back(triIdx);
            }
        }
    }
}

void Renderer::RenderTiles()
{
    uint numTiles = static_cast<uint>(tiles.size());
    std::atomic<int> tileIndex(0);

    auto worker = [this, &tileIndex, numTiles]()
    {
        while (true)
        {
            uint idx = static_cast<uint>(tileIndex.fetch_add(1));
            if (idx >= numTiles)
                break;

            const Tile& tile = tiles[idx];
            for (int triIdx : tile.triangleIndices)
            {
                const int3& tri = (*triangles)[triIdx];

                rasterizer.RasterizeTriangle(
                    (*verts)[tri.x],
                    (*verts)[tri.y],
                    (*verts)[tri.z],
                    state,
                    depthBuffer,
                    renderTarget,
                    pixelShader,
                    constantBuffer,
                    textureTable,
                    tile.min,
                    tile.max);
            }
        }
    };

    auto& pool = ThreadPoolManager::Get();
    for (uint i = 0; i < pool.threadCount(); ++i)
        pool.enqueue(worker);
    pool.wait();
}

SOFTX_END
