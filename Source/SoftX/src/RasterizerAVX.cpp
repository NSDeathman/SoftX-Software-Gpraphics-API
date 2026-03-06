#include "pch.h"

#include <SoftX/SoftX.h>
#include "RasterizerCommon.h"

SOFTX_BEGIN

void RasterizerAVX::RasterizeTriangle(
    const VertexOutput& v0,
    const VertexOutput& v1,
    const VertexOutput& v2,
    const RasterizerState& state,
    DepthBuffer& depthBuffer,
    IRenderTarget& renderTarget,
    const PixelShader& ps,
    const ConstantBuffer& cb,
    int2 tileMin,
    int2 tileMax)
{
    PROFILE_SCOPE("RasterizerAVX::RasterizeTriangle");

    int width = renderTarget.width();
    int height = renderTarget.height();

    // Bounding box треугольника
    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    // Ограничиваем bounding box тайлом
    int iMinX = std::max(tileMin.x, (int)std::floor(minX));
    int iMaxX = std::min(tileMax.x, (int)std::ceil(maxX));
    int iMinY = std::max(tileMin.y, (int)std::floor(minY));
    int iMaxY = std::min(tileMax.y, (int)std::ceil(maxY));

    if (iMinX > iMaxX || iMinY > iMaxY) return;

    float area2 = RasterizerCommon::edgeFunction(v0.Position, v1.Position, v2.Position);
    CullMode cull = state.cullMode;
    if (cull == CullMode::Back && area2 < 0) return;
    if (cull == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) return;

    // Предвычисленные константы (аналогично RasterizeTriangle)
    float4 dx01 = v1.Position - v0.Position;
    float4 dx12 = v2.Position - v1.Position;
    float4 dx20 = v0.Position - v2.Position;

    // Размножение атрибутов (как в RasterizeTriangle)
    __m256 v0x = _mm256_set1_ps(v0.Position.x);
    __m256 v0y = _mm256_set1_ps(v0.Position.y);
    __m256 v1x = _mm256_set1_ps(v1.Position.x);
    __m256 v1y = _mm256_set1_ps(v1.Position.y);
    __m256 v2x = _mm256_set1_ps(v2.Position.x);
    __m256 v2y = _mm256_set1_ps(v2.Position.y);

    __m256 v0z = _mm256_set1_ps(v0.Position.z);
    __m256 v1z = _mm256_set1_ps(v1.Position.z);
    __m256 v2z = _mm256_set1_ps(v2.Position.z);

    __m256 v0cr = _mm256_set1_ps(v0.Color.x);
    __m256 v0cg = _mm256_set1_ps(v0.Color.y);
    __m256 v0cb = _mm256_set1_ps(v0.Color.z);
    __m256 v0ca = _mm256_set1_ps(v0.Color.w);
    __m256 v1cr = _mm256_set1_ps(v1.Color.x);
    __m256 v1cg = _mm256_set1_ps(v1.Color.y);
    __m256 v1cb = _mm256_set1_ps(v1.Color.z);
    __m256 v1ca = _mm256_set1_ps(v1.Color.w);
    __m256 v2cr = _mm256_set1_ps(v2.Color.x);
    __m256 v2cg = _mm256_set1_ps(v2.Color.y);
    __m256 v2cb = _mm256_set1_ps(v2.Color.z);
    __m256 v2ca = _mm256_set1_ps(v2.Color.w);

    __m256 v0nx = _mm256_set1_ps(v0.Normal.x);
    __m256 v0ny = _mm256_set1_ps(v0.Normal.y);
    __m256 v0nz = _mm256_set1_ps(v0.Normal.z);
    __m256 v1nx = _mm256_set1_ps(v1.Normal.x);
    __m256 v1ny = _mm256_set1_ps(v1.Normal.y);
    __m256 v1nz = _mm256_set1_ps(v1.Normal.z);
    __m256 v2nx = _mm256_set1_ps(v2.Normal.x);
    __m256 v2ny = _mm256_set1_ps(v2.Normal.y);
    __m256 v2nz = _mm256_set1_ps(v2.Normal.z);

    __m256 v0u = _mm256_set1_ps(v0.UV.x);
    __m256 v0v = _mm256_set1_ps(v0.UV.y);
    __m256 v1u = _mm256_set1_ps(v1.UV.x);
    __m256 v1v = _mm256_set1_ps(v1.UV.y);
    __m256 v2u = _mm256_set1_ps(v2.UV.x);
    __m256 v2v = _mm256_set1_ps(v2.UV.y);

    __m256 invArea = _mm256_set1_ps(1.0f / area2);
    __m256 dx01v = _mm256_set1_ps(dx01.x);
    __m256 dy01v = _mm256_set1_ps(dx01.y);
    __m256 dx12v = _mm256_set1_ps(dx12.x);
    __m256 dy12v = _mm256_set1_ps(dx12.y);
    __m256 dx20v = _mm256_set1_ps(dx20.x);
    __m256 dy20v = _mm256_set1_ps(dx20.y);

    for (int y = iMinY; y <= iMaxY; ++y)
    {
        __m256 baseY = _mm256_set1_ps(y + 0.5f);

        int x;
        for (x = iMinX; x <= iMaxX - 7; x += 8)
        {
            // Проверка, пересекается ли блок из 8 пикселей с тайлом
            if (x > tileMax.x || x + 7 < tileMin.x)
                continue;

            __m256 baseX = _mm256_set_ps(x + 7.5f, x + 6.5f, x + 5.5f, x + 4.5f,
                                          x + 3.5f, x + 2.5f, x + 1.5f, x + 0.5f);

            __m256 f01 = _mm256_sub_ps(
                _mm256_mul_ps(_mm256_sub_ps(baseX, v0x), dy01v),
                _mm256_mul_ps(_mm256_sub_ps(baseY, v0y), dx01v));
            __m256 f12 = _mm256_sub_ps(
                _mm256_mul_ps(_mm256_sub_ps(baseX, v1x), dy12v),
                _mm256_mul_ps(_mm256_sub_ps(baseY, v1y), dx12v));
            __m256 f20 = _mm256_sub_ps(
                _mm256_mul_ps(_mm256_sub_ps(baseX, v2x), dy20v),
                _mm256_mul_ps(_mm256_sub_ps(baseY, v2y), dx20v));

            __m256 zero = _mm256_setzero_ps();
            __m256 inside;
            if (area2 > 0)
            {
                inside = _mm256_and_ps(_mm256_and_ps(_mm256_cmp_ps(f01, zero, _CMP_GE_OQ),
                                                     _mm256_cmp_ps(f12, zero, _CMP_GE_OQ)),
                                       _mm256_cmp_ps(f20, zero, _CMP_GE_OQ));
            }
            else
            {
                inside = _mm256_and_ps(_mm256_and_ps(_mm256_cmp_ps(f01, zero, _CMP_LE_OQ),
                                                     _mm256_cmp_ps(f12, zero, _CMP_LE_OQ)),
                                       _mm256_cmp_ps(f20, zero, _CMP_LE_OQ));
            }
            int insideMask = _mm256_movemask_ps(inside);
            if (insideMask == 0) continue;

            __m256 alpha = _mm256_mul_ps(f12, invArea);
            __m256 beta  = _mm256_mul_ps(f20, invArea);
            __m256 gamma = _mm256_mul_ps(f01, invArea);

            __m256 z = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0z),
                                                   _mm256_mul_ps(beta, v1z)),
                                     _mm256_mul_ps(gamma, v2z));

            __m256 r = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0cr),
                                                   _mm256_mul_ps(beta, v1cr)),
                                     _mm256_mul_ps(gamma, v2cr));
            __m256 g = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0cg),
                                                   _mm256_mul_ps(beta, v1cg)),
                                     _mm256_mul_ps(gamma, v2cg));
            __m256 b = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0cb),
                                                   _mm256_mul_ps(beta, v1cb)),
                                     _mm256_mul_ps(gamma, v2cb));
            __m256 a = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0ca),
                                                   _mm256_mul_ps(beta, v1ca)),
                                     _mm256_mul_ps(gamma, v2ca));

            __m256 nx = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0nx),
                                                    _mm256_mul_ps(beta, v1nx)),
                                      _mm256_mul_ps(gamma, v2nx));
            __m256 ny = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0ny),
                                                    _mm256_mul_ps(beta, v1ny)),
                                      _mm256_mul_ps(gamma, v2ny));
            __m256 nz = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0nz),
                                                    _mm256_mul_ps(beta, v1nz)),
                                      _mm256_mul_ps(gamma, v2nz));

            __m256 u = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0u),
                                                   _mm256_mul_ps(beta, v1u)),
                                     _mm256_mul_ps(gamma, v2u));
            __m256 v = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(alpha, v0v),
                                                   _mm256_mul_ps(beta, v1v)),
                                     _mm256_mul_ps(gamma, v2v));

            int idx0 = y * width + x;
            __m256 depths = _mm256_loadu_ps(&depthBuffer.at(idx0));

            __m256 depthCmp = _mm256_cmp_ps(z, depths, _CMP_LT_OQ);
            int depthMask = _mm256_movemask_ps(depthCmp) & insideMask;
            if (depthMask == 0) continue;

            alignas(32) float zArr[8], rArr[8], gArr[8], bArr[8], aArr[8];
            alignas(32) float nxArr[8], nyArr[8], nzArr[8];
            alignas(32) float uArr[8], vArr[8];
            _mm256_store_ps(zArr, z);
            _mm256_store_ps(rArr, r);
            _mm256_store_ps(gArr, g);
            _mm256_store_ps(bArr, b);
            _mm256_store_ps(aArr, a);
            _mm256_store_ps(nxArr, nx);
            _mm256_store_ps(nyArr, ny);
            _mm256_store_ps(nzArr, nz);
            _mm256_store_ps(uArr, u);
            _mm256_store_ps(vArr, v);

            for (int i = 0; i < 8; ++i)
            {
                int px = x + i;
                if (px < tileMin.x || px > tileMax.x)
                    continue;

                if (depthMask & (1 << i))
                {
                    int py = y;
                    int idx = py * width + px;

                    depthBuffer.at(idx) = zArr[i];

                    VertexOutput frag;
                    frag.Position = float4((float)px, (float)py, zArr[i], 1.0f);
                    frag.Color = float4(rArr[i], gArr[i], bArr[i], aArr[i]);
                    frag.Normal = float3(nxArr[i], nyArr[i], nzArr[i]);
                    frag.UV = float2(uArr[i], vArr[i]);

                    float4 finalColor = ps(frag, cb);
                    renderTarget.set_pixel(int2(px, py), finalColor);
                }
            }
        }

        // Скалярный доводчик для оставшихся пикселей
        for (; x <= iMaxX; ++x)
        {
            if (x < tileMin.x || x > tileMax.x)
                continue;

            float2 p((float)x + 0.5f, (float)y + 0.5f);
			float f0 = RasterizerCommon::edgeFunction(v1.Position, v2.Position, p);
			float f1 = RasterizerCommon::edgeFunction(v2.Position, v0.Position, p);
			float f2 = RasterizerCommon::edgeFunction(v0.Position, v1.Position, p);

            if ((area2 > 0 && (f0 < 0 || f1 < 0 || f2 < 0)) ||
                (area2 < 0 && (f0 > 0 || f1 > 0 || f2 > 0)))
            {
                continue;
            }

            float a = f0 / area2;
            float b = f1 / area2;
            float c = f2 / area2;

            VertexOutput frag = RasterizerCommon::trilerp(v0, v1, v2, a, b, c);

            int idx = y * width + x;
            if (frag.Position.z < depthBuffer.at(idx))
            {
                depthBuffer.at(idx) = frag.Position.z;
                float4 finalColor = ps(frag, cb);
                renderTarget.set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

SOFTX_END
