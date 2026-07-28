/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include "../include/SoftX.h"
#include "Rasterizer.h"
#include "TileGrid.h"
#include "ThreadPoolManager.h"
#include "ThreadUtils.h"
/////////////////////////////////////////////////////////////////
//#define DEBUG_TILING
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

void DeviceContext::DrawPoint(Texture& rt, DepthBuffer& db, const RasterizerState& rasterState, int x, int y, float z, const float4& color)
{
    const uint w = rt.Width();
    const uint h = rt.Height();
    if (x < 0 || y < 0 || x >= static_cast<int>(w) || y >= static_cast<int>(h))
        return;

    uint idx = y * w + x;
    float depthValue = db.At(idx);

    if (RasterizerCommon::DepthTest(z, depthValue, rasterState.depthFunc))
    {
        if (rasterState.depthWriteEnable)
            db.At(idx) = z;

        __m128 col = _mm_set_ps(color.w, color.z, color.y, color.x);
        rt.StreamWrite(uint2(x, y), col);
    }
}

void DeviceContext::DrawLine(Texture& rt, DepthBuffer& db, const RasterizerState& rasterState, int x0, int y0, int x1, int y1, float z0, float z1, const float4& color)
{
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int steps = std::max(dx, -dy);
    float zStep = (steps > 0) ? (z1 - z0) / static_cast<float>(steps) : 0.0f;
    float z = z0;
    int x = x0, y = y0;

    for (int i = 0; i <= steps; ++i)
    {
        DrawPoint(rt, db, rasterState, x, y, z, color);
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
        z += zStep;
    }
}

std::vector<Interpolant> DeviceContext::ProcessIndexedVertices(const PipelineStateObject& state,
                                                               uint indexCount, 
                                                               uint startIndex,
                                                               uint totalVertices)
{
    PROFILE_SCOPE("Vertex Shader (indexed)");
    std::vector<Interpolant> clipVerts(totalVertices);

    std::vector<uint> uniqueIndices;
    {
        std::vector<bool> visited(totalVertices, false);
        for (uint i = startIndex; i < startIndex + indexCount; ++i)
        {
            uint idx = state.indexBuffer.GetByIndex(i);
            if (!visited[idx])
            {
                visited[idx] = true;
                uniqueIndices.push_back(idx);
            }
        }
    }

    const size_t totalUnique = uniqueIndices.size();
    ThreadUtils::SmartParallelFor(size_t(0), totalUnique, size_t(1),
    [&](size_t i)
    {
        uint idx = uniqueIndices[i];
        clipVerts[idx] = state.vertexShader(
            state.vertexBuffer.GetByIndex(idx),
            state.constantBuffer,
            state.textureTable);
    });

    return clipVerts;
}

std::vector<int3> DeviceContext::GatherIndexedTriangles(const PipelineStateObject& state,
                                                        uint indexCount, 
                                                        uint startIndex)
{
    PROFILE_SCOPE("Gather indexed triangles");
    const uint triangleCount = indexCount / 3;
    std::vector<int3> triangles;
    triangles.reserve(triangleCount);

    for (uint i = startIndex; i + 2 < startIndex + indexCount; i += 3)
    {
        triangles.emplace_back(static_cast<int>(state.indexBuffer.GetByIndex(i)),
                               static_cast<int>(state.indexBuffer.GetByIndex(i + 1)),
                               static_cast<int>(state.indexBuffer.GetByIndex(i + 2)));
    }
    return triangles;
}

std::vector<Interpolant> DeviceContext::ProcessNonIndexedVertices(const PipelineStateObject& state,
                                                                  uint vertexCount, 
                                                                  uint startVertex)
{
    PROFILE_SCOPE("Vertex Shader (non-indexed)");
    std::vector<Interpolant> clipVerts(vertexCount);

    ThreadUtils::SmartParallelFor(uint(0), vertexCount, uint(1),
    [&](uint i)
    {
        uint idx = startVertex + i;
        clipVerts[i] = state.vertexShader(
            state.vertexBuffer.GetByIndex(idx),
            state.constantBuffer,
            state.textureTable);
    });

    return clipVerts;
}

std::vector<int3> DeviceContext::GatherNonIndexedTriangles(uint vertexCount)
{
    const uint triangleCount = vertexCount / 3;
    std::vector<int3> triangles;
    triangles.reserve(triangleCount);
    for (uint i = 0; i + 2 < vertexCount; i += 3)
        triangles.emplace_back(i, i + 1, i + 2);
    return triangles;
}

void DeviceContext::ClipAndRasterize(const PipelineStateObject& state,
                                     std::vector<Interpolant>& clipVerts,
                                     const std::vector<int3>& sourceTriangles)
{
    std::vector<Interpolant> finalVerts;
    std::vector<int3> finalTriangles;
    finalVerts.reserve(sourceTriangles.size() * 3);
    finalTriangles.reserve(sourceTriangles.size() * 2);

    {
        PROFILE_SCOPE("Near plane clipping");
        for (const auto& tri : sourceTriangles)
        {
            Interpolant clipped[2][3];
            int numTris = RasterizerCommon::ClipTriangleNearPlane(clipVerts[tri.x], clipVerts[tri.y], clipVerts[tri.z], clipped);
            for (int t = 0; t < numTris; ++t)
            {
                int base = static_cast<int>(finalVerts.size());
                finalVerts.push_back(clipped[t][0]);
                finalVerts.push_back(clipped[t][1]);
                finalVerts.push_back(clipped[t][2]);
                finalTriangles.emplace_back(base, base + 1, base + 2);
            }
        }
    }
    if (finalTriangles.empty())
        return;

    {
        PROFILE_SCOPE("Perspective divide");
        for (auto& v : finalVerts)
            RasterizerCommon::ClipSpaceToScreenSpace(v, state.viewport);
    }

    if (state.geometryShader)
    {
        PROFILE_SCOPE("Geometry shader");
        std::vector<Interpolant> gsVerts;
        std::vector<int3> gsTriangles;
        gsVerts.reserve(finalTriangles.size() * 6);
        gsTriangles.reserve(finalTriangles.size() * 2);

        for (const auto& tri : finalTriangles)
        {
            Interpolant inVerts[3] = { finalVerts[tri.x], finalVerts[tri.y], finalVerts[tri.z] };
            std::vector<Interpolant> outVerts;
            std::vector<int> outIndices;
            state.geometryShader(inVerts, outVerts, outIndices, state.textureTable);

            int base = static_cast<int>(gsVerts.size());
            gsVerts.insert(gsVerts.end(), outVerts.begin(), outVerts.end());
            for (size_t j = 0; j + 2 < outIndices.size(); j += 3)
                gsTriangles.emplace_back(base + outIndices[j], base + outIndices[j + 1], base + outIndices[j + 2]);
        }
        finalVerts = std::move(gsVerts);
        finalTriangles = std::move(gsTriangles);
    }

    Texture* rt = state.renderTarget.get();
    DepthBuffer* db = state.depthBuffer.get();

    RasterizerState fullRasterState;
    fullRasterState.cullMode = state.cullMode;
    fullRasterState.fillMode = state.fillMode;
    fullRasterState.depthFunc = state.depthFunc;
    fullRasterState.depthWriteEnable = state.depthWriteEnable;

    if (state.fillMode == FillMode::Solid)
    {
        PROFILE_SCOPE("Create TriangleSetups");
        RasterizerState cullRasterState;
        cullRasterState.cullMode = state.cullMode;

        std::vector<RasterizerCommon::TriangleSetup> setups;
        setups.reserve(finalTriangles.size());

        uint64_t totalPixelCoverage = 0;
        constexpr uint64_t PIXEL_COVERAGE_THRESHOLD = 4096;

        for (const auto& tri : finalTriangles)
        {
            auto optSetup = RasterizerCommon::CreateTriangleSetup(finalVerts[tri.x], finalVerts[tri.y], finalVerts[tri.z], cullRasterState);
            if (optSetup)
            {
                const auto& s = *optSetup;
                totalPixelCoverage += static_cast<uint64_t>(s.bbMaxX - s.bbMinX + 1) * static_cast<uint64_t>(s.bbMaxY - s.bbMinY + 1);
                setups.push_back(std::move(*optSetup));
            }
        }

        if (!setups.empty())
        {
            if (totalPixelCoverage < PIXEL_COVERAGE_THRESHOLD)
            {
                PROFILE_SCOPE("Render Solid (single-threaded)");
                const uint w = rt->Width();
                const uint h = rt->Height();
                for (const auto& s : setups)
                {
                    int minX = std::max(0, s.bbMinX);
                    int minY = std::max(0, s.bbMinY);
                    int maxX = std::min(static_cast<int>(w) - 1, s.bbMaxX);
                    int maxY = std::min(static_cast<int>(h) - 1, s.bbMaxY);

                    if (minX >= maxX || minY >= maxY) continue;

                    Rasterizer::RasterizeTriangle(s,
                                                  fullRasterState,
                                                  *db,
                                                  rt,
                                                  state.viewport,
                                                  state.pixelShader,
                                                  state.constantBuffer,
                                                  &state.textureTable,
                                                  uint2(minX, minY),
                                                  uint2(maxX, maxY));
                }
            }
            else
            {
                TileGrid tileGrid;
                {
                    PROFILE_SCOPE("Tile binning");
                    uint width  = rt->Width();
                    uint height = rt->Height();
                    tileGrid.Build(width, height, state.tileSize);
                    tileGrid.BinTriangles(setups);
                }

                {
                    PROFILE_SCOPE("Render Solid");
                    const auto& tiles = tileGrid.GetTiles();
                    uint numTiles = static_cast<uint>(tiles.size());
                    std::atomic<int> tileIndex(0);

                    auto Task = [&]()
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
                                Rasterizer::RasterizeTriangle(
                                    setups[triIdx],
                                    fullRasterState,
                                    *db,
                                    rt,
                                    state.viewport,
                                    state.pixelShader,
                                    state.constantBuffer,
                                    &state.textureTable,
                                    tile.min,
                                    tile.max);
                            }
                        }
                    };
                    ThreadUtils::DispatchWorkers(Task);
                }
#ifdef DEBUG_TILING
                DrawActiveTileBorders(*rt, fullRasterState, state.tileSize, tiles);
#endif
            }
        }
    }
    else if (state.fillMode == FillMode::Wireframe)
    {
        PROFILE_SCOPE("Render Wireframe");
        float4 wireColor(1.0f, 1.0f, 1.0f, 1.0f);
        for (const auto& tri : finalTriangles)
        {
            const auto& v0 = finalVerts[tri.x];
            const auto& v1 = finalVerts[tri.y];
            const auto& v2 = finalVerts[tri.z];
            float depth0 = RasterizerCommon::ComputeDepth(v0.Position.z, v0.Position.w, state.viewport);
            float depth1 = RasterizerCommon::ComputeDepth(v1.Position.z, v1.Position.w, state.viewport);
            float depth2 = RasterizerCommon::ComputeDepth(v2.Position.z, v2.Position.w, state.viewport);

            DrawLine(*rt, *db, fullRasterState,
                     (int)round(v0.Position.x), (int)round(v0.Position.y),
                     (int)round(v1.Position.x), (int)round(v1.Position.y),
                     depth0, depth1, wireColor);

            DrawLine(*rt, *db, fullRasterState,
                     (int)round(v1.Position.x), (int)round(v1.Position.y),
                     (int)round(v2.Position.x), (int)round(v2.Position.y),
                     depth1, depth2, wireColor);

            DrawLine(*rt, *db, fullRasterState,
                     (int)round(v2.Position.x), (int)round(v2.Position.y),
                     (int)round(v0.Position.x), (int)round(v0.Position.y),
                     depth2, depth0, wireColor);
        }
    }
    else if (state.fillMode == FillMode::Point)
    {
        PROFILE_SCOPE("Render Point");
        std::vector<bool> drawn(finalVerts.size(), false);
        for (const auto& tri : finalTriangles)
        {
            for (int idx : {tri.x, tri.y, tri.z})
            {
                if (!drawn[idx])
                {
                    drawn[idx] = true;
                    const auto& v = finalVerts[idx];
                    float depth = RasterizerCommon::ComputeDepth(v.Position.z, v.Position.w, state.viewport);
                    DrawPoint(*rt, *db, fullRasterState, (int)round(v.Position.x), (int)round(v.Position.y), depth, v.Color);
                }
            }
        }
    }
}

void DeviceContext::Draw(uint vertexCount, uint startVertex)
{
    std::lock_guard<std::mutex> lock(drawMutex);
    PROFILE_SCOPE("DeviceContext::Draw (non-indexed)");

    CommitState();
    PipelineStateObject state = frontState;

    state.Validate(PipelineResource::VertexShader |
                   PipelineResource::VertexBuffer |
                   PipelineResource::DepthBuffer |
                   PipelineResource::Viewport |
                   PipelineResource::TileSize);

    if (vertexCount == 0 || startVertex + vertexCount > state.vertexBuffer.Size())
        SOFTX_THROW(InvalidArgument("Draw: vertexCount out of range"));

    DrawImpl(state, vertexCount, startVertex);
}

void DeviceContext::Draw()
{
    std::lock_guard<std::mutex> lock(drawMutex);
    PROFILE_SCOPE("DeviceContext::Draw (all vertices)");

    CommitState();
    PipelineStateObject state = frontState;

    state.Validate(PipelineResource::VertexShader |
                   PipelineResource::VertexBuffer |
                   PipelineResource::DepthBuffer |
                   PipelineResource::Viewport |
                   PipelineResource::TileSize);

    uint count = static_cast<uint>(state.vertexBuffer.Size());
    if (count == 0)
        SOFTX_THROW(InvalidState("Draw: vertex buffer is empty"));

    DrawImpl(state, count, 0);
}

void DeviceContext::DrawImpl(const PipelineStateObject& state, uint vertexCount, uint startVertex)
{
    auto clipVerts = ProcessNonIndexedVertices(state, vertexCount, startVertex);
    auto triangles = GatherNonIndexedTriangles(vertexCount);
    ClipAndRasterize(state, clipVerts, triangles);
}

void DeviceContext::DrawIndexed(uint indexCount, uint startIndex)
{
    std::lock_guard<std::mutex> lock(drawMutex);
    PROFILE_SCOPE("DeviceContext::DrawIndexed");

    CommitState();
    PipelineStateObject state = frontState;

    state.Validate(PipelineResource::VertexShader |
                   PipelineResource::VertexBuffer |
                   PipelineResource::IndexBuffer |
                   PipelineResource::DepthBuffer |
                   PipelineResource::Viewport |
                   PipelineResource::TileSize);

    DrawIndexedImpl(state, indexCount, startIndex);
}

void DeviceContext::DrawIndexed()
{
    std::lock_guard<std::mutex> lock(drawMutex);
    PROFILE_SCOPE("DeviceContext::DrawIndexed (full buffer)");

    CommitState();
    PipelineStateObject state = frontState;

    state.Validate(PipelineResource::VertexShader |
                   PipelineResource::VertexBuffer |
                   PipelineResource::IndexBuffer |
                   PipelineResource::DepthBuffer |
                   PipelineResource::Viewport |
                   PipelineResource::TileSize);

    uint count = static_cast<uint>(state.indexBuffer.Size());
    DrawIndexedImpl(state, count, 0);
}

void DeviceContext::DrawIndexedImpl(const PipelineStateObject& state, uint indexCount, uint startIndex)
{
    uint totalVertices = static_cast<uint>(state.vertexBuffer.Size());
    auto clipVerts = ProcessIndexedVertices(state, indexCount, startIndex, totalVertices);
    auto triangles = GatherIndexedTriangles(state, indexCount, startIndex);
    ClipAndRasterize(state, clipVerts, triangles);
}

void DeviceContext::DrawFullScreenQuad()
{
    std::lock_guard<std::mutex> lock(drawMutex);
    PROFILE_SCOPE("DeviceContext::DrawFullScreenQuad");

    CommitState();
    PipelineStateObject state = frontState;

    state.Validate(PipelineResource::RenderTarget |
                   PipelineResource::PixelShader |
                   PipelineResource::Viewport |
                   PipelineResource::TileSize);

    Texture* rt = state.renderTarget.get();
    const uint w = rt->Width();
    const uint h = rt->Height();
    const float invW = 1.0f / (w - 1u);
    const float invH = 1.0f / (h - 1u);

    __m128* pixels = rt->GetRawPixels();

    auto ps = state.pixelShader;
    auto cb = state.constantBuffer;
    auto tt = state.textureTable;

    const uint ts = state.tileSize;
    TileGrid tileGrid;
    tileGrid.Build(w, h, ts);
    const auto& tiles = tileGrid.GetTiles();
    uint numTiles = static_cast<uint>(tiles.size());
    std::atomic<uint> tileIndex(0);

    auto worker = [&]()
    {
        PROFILE_SCOPE("FullScreenQuad Worker");
        while (true)
        {
            uint idx = tileIndex.fetch_add(1);
            if (idx >= numTiles) break;
            const Tile& tile = tiles[idx];
            const uint startX = tile.min.x, endX = tile.max.x;
            const uint startY = tile.min.y, endY = tile.max.y;

            float v = startY * invH;
            for (uint y = startY; y <= endY; ++y, v += invH)
            {
                float u = startX * invW;
                uint x = startX;
                __m128* row = pixels + y * w;

                for (; x + 3 <= endX; x += 4, u += 4.0f * invW)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        Interpolant input;
                        input.UV = float2(u + i * invW, v);
                        float4 c = ps(input, cb, tt);
                        _mm_stream_ps(reinterpret_cast<float*>(row + x + i), _mm_set_ps(c.w, c.z, c.y, c.x));
                    }
                }

                for (; x <= endX; ++x, u += invW)
                {
                    Interpolant input;
                    input.UV = float2(u, v);
                    float4 c = ps(input, cb, tt);
                    _mm_stream_ps(reinterpret_cast<float*>(row + x), _mm_set_ps(c.w, c.z, c.y, c.x));
                }
            }
        }
    };

    ThreadUtils::DispatchWorkers(worker);
}

SOFTX_END
/////////////////////////////////////////////////////////////////
