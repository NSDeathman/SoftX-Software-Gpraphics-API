/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include "../include/SoftX.h"
#include "../include/RasterizerInterface.h"
#include "Renderer.h"
#include "ThreadPoolManager.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

Renderer::Renderer(IRasterizer& rasterizer) : rasterizer(rasterizer)
{
}

void Renderer::Execute(const PipelineStateObject& pso,
                      const std::vector<VertexOutput>& verts,
                      const std::vector<int3>& triangles)
{
    PROFILE_SCOPE("Renderer::Execute");

    if (pso.renderTarget)
    {
        width = pso.renderTarget->Width();
        height = pso.renderTarget->Height();
    }
    else if (pso.depthBuffer)
    {
        width = pso.depthBuffer->Width();
        height = pso.depthBuffer->Height();
    }
    else
    {
        return;
    }

    tileSize = pso.tileSize;

    BuildTiles(width, height, tileSize);
    BinTriangles(verts, triangles, width, height, tileSize);
    RenderTiles(pso, verts, triangles);
}


void Renderer::BuildTiles(uint global_width, uint global_height, uint global_tileSize)
{
    PROFILE_SCOPE("Renderer::BuildTiles");
    tiles.clear();
    uint ts = tileSize;
    uint tilesX = (global_width + ts - 1) / ts;
    uint tilesY = (global_height + ts - 1) / ts;
    for (uint ty = 0; ty < tilesY; ++ty)
    {
        for (uint tx = 0; tx < tilesX; ++tx)
        {
            uint2 mn(tx * ts, ty * ts);
            uint2 mx(std::min((tx + 1) * ts - 1, global_width - 1),
                     std::min((ty + 1) * ts - 1, global_height - 1));
            tiles.emplace_back(mn, mx);
        }
    }
}

void Renderer::BinTriangles(const std::vector<VertexOutput>& inputVerts,
                            const std::vector<int3>& inputTriangles,
                            uint width,
                            uint height,
                            uint tileSize)
{
    PROFILE_SCOPE("Renderer::BinTriangles");
    for (auto& t : tiles)
        t.triangleIndices.clear();

    uint ts = tileSize;
    uint tilesX = (width + ts - 1) / ts;
    uint tilesY = (height + ts - 1) / ts;
    float rtWidthF = (float)width - 1.0f;
    float rtHeightF = (float)height - 1.0f;

    for (int triIdx = 0; triIdx < static_cast<int>(inputTriangles.size()); ++triIdx)
    {
        const auto& tri = inputTriangles[triIdx];
        const auto& v0 = inputVerts[tri.x];
        const auto& v1 = inputVerts[tri.y];
        const auto& v2 = inputVerts[tri.z];

        float x0 = AfterMath::clamp(v0.Position.x, 0.0f, rtWidthF);
        float y0 = AfterMath::clamp(v0.Position.y, 0.0f, rtHeightF);
        float x1 = AfterMath::clamp(v1.Position.x, 0.0f, rtWidthF);
        float y1 = AfterMath::clamp(v1.Position.y, 0.0f, rtHeightF);
        float x2 = AfterMath::clamp(v2.Position.x, 0.0f, rtWidthF);
        float y2 = AfterMath::clamp(v2.Position.y, 0.0f, rtHeightF);

        float minX = std::min({ x0, x1, x2 });
        float maxX = std::max({ x0, x1, x2 });
        float minY = std::min({ y0, y1, y2 });
        float maxY = std::max({ y0, y1, y2 });

        if (minX >= rtWidthF || maxX <= 0.0f || minY >= rtHeightF || maxY <= 0.0f)
            continue;

        int tileX0 = AfterMath::clamp(static_cast<int>(std::floor(minX)) / static_cast<int>(ts), 0, static_cast<int>(tilesX) - 1);
        int tileY0 = AfterMath::clamp(static_cast<int>(std::floor(minY)) / static_cast<int>(ts), 0, static_cast<int>(tilesY) - 1);
        int tileX1 = AfterMath::clamp(static_cast<int>(std::ceil(maxX)) / static_cast<int>(ts), 0, static_cast<int>(tilesX) - 1);
        int tileY1 = AfterMath::clamp(static_cast<int>(std::ceil(maxY)) / static_cast<int>(ts), 0, static_cast<int>(tilesY) - 1);

        for (int ty = tileY0; ty <= tileY1; ++ty)
        {
            for (int tx = tileX0; tx <= tileX1; ++tx)
            {
                tiles[ty * tilesX + tx].triangleIndices.push_back(triIdx);
            }
        }
    }
}

void Renderer::RenderTiles(const PipelineStateObject& pso,
                           const std::vector<VertexOutput>& verts,
                           const std::vector<int3>& triangles)
{
    PROFILE_SCOPE("Renderer::RenderTiles");

    RasterizerState rasterState;
    rasterState.cullMode = pso.cullMode;
    rasterState.fillMode = pso.fillMode;
    rasterState.depthFunc = pso.depthFunc;
    rasterState.depthWriteEnable = pso.depthWriteEnable;

    uint numTiles = static_cast<uint>(tiles.size());
    std::atomic<int> tileIndex(0);

    auto worker = [&]()
    {
        PROFILE_SCOPE("RenderTiles::tile worker");
        while (true)
        {
            uint idx = static_cast<uint>(tileIndex.fetch_add(1));
            if (idx >= numTiles)
                break;

            const Tile& tile = tiles[idx];
            for (int triIdx : tile.triangleIndices)
            {
                const int3& tri = triangles[triIdx];

                rasterizer.RasterizeTriangle(verts[tri.x],
                                             verts[tri.y],
                                             verts[tri.z],
                                             rasterState,
                                             *pso.depthBuffer,
                                             pso.renderTarget.get(),
                                             pso.pixelShader,
                                             pso.constantBuffer,
                                             &pso.textureTable,
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
/////////////////////////////////////////////////////////////////
