#include "pch.h"

#include <ppl.h>

#include <SoftX/SoftX.h>
#include <SoftX/ThreadPoolManager.h>

SOFTX_BEGIN

void DeviceContext::DrawPoint(int x, int y, float z, const float4& color)
{
	PROFILE_SCOPE("DeviceContext::DrawPoint");

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
	PROFILE_SCOPE("DeviceContext::DrawLine");

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

float4 DeviceContext::ClipToScreen(const float4& clipPos) const
{
	Viewport vp = m_Viewport;

	float invW = 1.0f / clipPos.w;
	float xNDC = clipPos.x * invW;
	float yNDC = clipPos.y * invW;
	float zNDC = clipPos.z * invW;

	float screenX = vp.pos.x + (xNDC * 0.5f + 0.5f) * vp.size.x;
	float screenY = vp.pos.y + (1.0f - (yNDC * 0.5f + 0.5f)) * vp.size.y;
	float screenZ = vp.minZ + zNDC * (vp.maxZ - vp.minZ);

	return float4(screenX, screenY, screenZ, 1.0f);
}

VertexOutput DeviceContext::trilerp(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float a, float b, float c)
{
	VertexOutput result;

#define TRILERP(field) result.field = a * v0.field + b * v1.field + c * v2.field

	TRILERP(Position);
	TRILERP(Normal);
	TRILERP(Color);
	TRILERP(UV);

#undef TRILERP

	return result;
}

void DeviceContext::RasterizeTriangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2)
{
	PROFILE_SCOPE("DeviceContext::RasterizeTriangle");

    IRenderTarget* rt = m_RenderTarget;
    if (!rt) return;

    if (!m_DepthBuffer)
		return;

    int width = rt->width();
    int height = rt->height();

    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    int iMinX = std::max(0, (int)std::floor(minX));
    int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
    int iMinY = std::max(0, (int)std::floor(minY));
    int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

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

    for (int y = iMinY; y <= iMaxY; ++y)
    {
        for (int x = iMinX; x <= iMaxX; ++x)
        {
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

                float4 finalColor = ps(frag, cb);

                rt->set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

void DeviceContext::RasterizeTriangleSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2)
{
	PROFILE_SCOPE("DeviceContext::RasterizeTriangleSSE");

    IRenderTarget* rt = m_RenderTarget;
    if (!rt) return;
    int width = rt->width();
    int height = rt->height();

    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    int iMinX = std::max(0, (int)std::floor(minX));
    int iMaxX = std::min(width - 1, (int)std::ceil(maxX));
    int iMinY = std::max(0, (int)std::floor(minY));
    int iMaxY = std::min(height - 1, (int)std::ceil(maxY));

    float area2 = edgeFunction(v0.Position, v1.Position, v2.Position);
    CullMode cull = m_cullMode;
    if (cull == CullMode::Back && area2 < 0) return;
    if (cull == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) return;

    auto ps = m_PixelShader;
    auto cb = m_ConstantBuffer;

    float4 dx01_ = v1.Position - v0.Position;
    float4 dx12_ = v2.Position - v1.Position;
    float4 dx20_ = v0.Position - v2.Position;

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

    __m128 v0nx = _mm_set1_ps(v0.Normal.x);
	__m128 v0ny = _mm_set1_ps(v0.Normal.y);
	__m128 v0nz = _mm_set1_ps(v0.Normal.z);

	__m128 v1nx = _mm_set1_ps(v1.Normal.x);
	__m128 v1ny = _mm_set1_ps(v1.Normal.y);
	__m128 v1nz = _mm_set1_ps(v1.Normal.z);

	__m128 v2nx = _mm_set1_ps(v2.Normal.x);
	__m128 v2ny = _mm_set1_ps(v2.Normal.y);
	__m128 v2nz = _mm_set1_ps(v2.Normal.z);

    __m128 v0u = _mm_set1_ps(v0.UV.x);
    __m128 v0v = _mm_set1_ps(v0.UV.y);
    __m128 v1u = _mm_set1_ps(v1.UV.x);
    __m128 v1v = _mm_set1_ps(v1.UV.y);
    __m128 v2u = _mm_set1_ps(v2.UV.x);
    __m128 v2v = _mm_set1_ps(v2.UV.y);

    __m128 invArea = _mm_set1_ps(1.0f / area2);
    __m128 dx01v = _mm_set1_ps(dx01_.x);
    __m128 dy01v = _mm_set1_ps(dx01_.y);
    __m128 dx12v = _mm_set1_ps(dx12_.x);
    __m128 dy12v = _mm_set1_ps(dx12_.y);
    __m128 dx20v = _mm_set1_ps(dx20_.x);
    __m128 dy20v = _mm_set1_ps(dx20_.y);

    for (int y = iMinY; y <= iMaxY; ++y)
    {
        __m128 baseY = _mm_set1_ps(y + 0.5f);

        int x;
        for (x = iMinX; x <= iMaxX - 3; x += 4)
        {
            __m128 baseX = _mm_set_ps(x + 3.5f, x + 2.5f, x + 1.5f, x + 0.5f);

            __m128 f01 = _mm_sub_ps(
                _mm_mul_ps(_mm_sub_ps(baseX, v0x), dy01v),
                _mm_mul_ps(_mm_sub_ps(baseY, v0y), dx01v));
            __m128 f12 = _mm_sub_ps(
                _mm_mul_ps(_mm_sub_ps(baseX, v1x), dy12v),
                _mm_mul_ps(_mm_sub_ps(baseY, v1y), dx12v));
            __m128 f20 = _mm_sub_ps(
                _mm_mul_ps(_mm_sub_ps(baseX, v2x), dy20v),
                _mm_mul_ps(_mm_sub_ps(baseY, v2y), dx20v));

            __m128 zero = _mm_setzero_ps();
            __m128 inside;
            if (area2 > 0)
            {
                inside = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(f01, zero), _mm_cmpge_ps(f12, zero)), _mm_cmpge_ps(f20, zero));
            }
            else
            {
                inside = _mm_and_ps(_mm_and_ps(_mm_cmple_ps(f01, zero), _mm_cmple_ps(f12, zero)), _mm_cmple_ps(f20, zero));
            }
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

            __m128 nx = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0nx), _mm_mul_ps(beta, v1nx)), _mm_mul_ps(gamma, v2nx));
            __m128 ny = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0ny), _mm_mul_ps(beta, v1ny)), _mm_mul_ps(gamma, v2ny));
            __m128 nz = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0nz), _mm_mul_ps(beta, v1nz)), _mm_mul_ps(gamma, v2nz));

            int idx0 = y * width + x;
            __m128 depths = _mm_loadu_ps(&m_DepthBuffer->at(idx0));

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

            float nxArr[4], nyArr[4], nzArr[4];
			_mm_storeu_ps(nxArr, nx);
			_mm_storeu_ps(nyArr, ny);
			_mm_storeu_ps(nzArr, nz);

            for (int i = 0; i < 4; ++i)
            {
                if (depthMask & (1 << i))
                {
                    int px = x + i;
                    int py = y;
                    int idx = py * width + px;

                    m_DepthBuffer->at(idx) = zArr[i];

                    VertexOutput frag;
                    frag.Position = float4((float)px, (float)py, zArr[i], 1.0f);
                    frag.Color = float4(rArr[i], gArr[i], bArr[i], aArr[i]);
					frag.Normal = float3(nxArr[i], nyArr[i], nzArr[i]);
                    frag.UV = float2(uArr[i], vArr[i]);

                    float4 finalColor = ps(frag, cb);
                    rt->set_pixel(int2(px, py), finalColor);
                }
            }
        }

        for (; x <= iMaxX; ++x)
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
        out.Position = ClipToScreen(out.Position);
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
        if (m_EnableTiledRendering) {
            buildTiles(width, height);
            binTriangles(m_transformedVerts, m_triangles);
            renderTilesMultithreaded();
        } else {
            for (const auto& tri : m_triangles) {
				RasterizeTriangleSSE(m_transformedVerts[tri.x], m_transformedVerts[tri.y], m_transformedVerts[tri.z]);
            }
        }
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

    if (m_EnableTiledRendering) {
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
    } else {
        VertexOutput input;
        for (int y = 0; y < h; ++y) {
            float v = y * invH;
            for (int x = 0; x < w; ++x) {
                float u = x * invW;
                input.UV = float2(u, v);
                float4 color = m_PixelShader(input, m_ConstantBuffer);
                rt->set_pixel(int2(x, y), color);
            }
        }
    }
}

SOFTX_END
