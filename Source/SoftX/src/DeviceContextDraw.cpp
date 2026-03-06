#include "pch.h"

#include <ppl.h>

#include <SoftX/SoftX.h>
#include <SoftX/ThreadPoolManager.h>
#include "RasterizerCommon.h"

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

    m_transformedVerts.clear();
    m_triangles.clear();

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

    m_transformedVerts.resize(m_VertexBuffer.Size());

    concurrency::parallel_for_each(uniqueIndices.begin(), uniqueIndices.end(), [&](uint32_t idx) {
        VertexOutput out = m_VertexShader(m_VertexBuffer.GetByIndex(idx), m_ConstantBuffer);
		out.Position = RasterizerCommon::ClipSpaceToScreenSpace(out.Position, m_Viewport);
        m_transformedVerts[idx] = out;
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
                m_transformedVerts[tri.x],
                m_transformedVerts[tri.y],
                m_transformedVerts[tri.z]
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

        m_transformedVerts = std::move(finalVerts);
        m_triangles = std::move(finalTriangles);
    } else {
        m_triangles = std::move(sourceTriangles);
    }

    if (m_fillMode == FillMode::Solid) {
        buildTiles(width, height);
        binTriangles(m_transformedVerts, m_triangles);
        renderTiles();
#ifdef DEBUG_TILING
		DrawActiveTileBorders();
#endif
    } else if (m_fillMode == FillMode::Wireframe) {
        float4 wireColor(1,1,1,1);
        for (const auto& tri : m_triangles) {
            const auto& v0 = m_transformedVerts[tri.x];
            const auto& v1 = m_transformedVerts[tri.y];
            const auto& v2 = m_transformedVerts[tri.z];
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
        std::vector<bool> drawn(m_transformedVerts.size(), false);
        for (const auto& tri : m_triangles) {
            for (int idx : {tri.x, tri.y, tri.z}) {
                if (!drawn[idx]) {
                    drawn[idx] = true;
                    const auto& v = m_transformedVerts[idx];
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

void DeviceContext::DrawFullScreenQuad() {
	PROFILE_SCOPE("DeviceContext::DrawFullScreenQuad");

    if (!m_PixelShader || !m_RenderTarget) return;

    IRenderTarget* rt = m_RenderTarget;
    int w = rt->width();
    int h = rt->height();
    float invW = 1.0f / (w - 1);
    float invH = 1.0f / (h - 1);

    buildTiles(w, h);
    int numTiles = (int)m_tiles.size();
    std::atomic<int> tileIndex(0);

    auto worker = [this, invW, invH, &tileIndex, numTiles]() {
        while (true) {
            int idx = tileIndex.fetch_add(1);
            if (idx >= numTiles) break;
            renderTileQuad(idx, invW, invH);
        }
    };

    auto& pool = ThreadPoolManager::Get();
    int numThreads = (int)pool.threadCount();
    for (int i = 0; i < numThreads; ++i) {
        pool.enqueue(worker);
    }
    pool.wait();
#ifdef DEBUG_TILING
    DrawActiveTileBorders();
#endif
}

SOFTX_END
