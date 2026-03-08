#include "pch.h"

#include <SoftX/SoftX.h>
#include "RasterizerCommon.h"
#include <xmmintrin.h>

SOFTX_BEGIN

void RasterizerSSE::RasterizeTriangle(const VertexOutput& v0,
                                      const VertexOutput& v1,
                                      const VertexOutput& v2,
                                      const RasterizerState& state,
                                      DepthBuffer& depthBuffer,
                                      IRenderTarget& renderTarget,
                                      const PixelShader& ps,
                                      const ConstantBuffer& cb,
                                      const TextureTable* tt,
                                      int2 tileMin,
                                      int2 tileMax)
{
    PROFILE_SCOPE("RasterizerSSE::RasterizeTriangleTile");

    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    uint iMinX = std::max(tileMin.x, (int)std::floor(minX));
    uint iMaxX = std::min(tileMax.x, (int)std::ceil(maxX));
    uint iMinY = std::max(tileMin.y, (int)std::floor(minY));
    uint iMaxY = std::min(tileMax.y, (int)std::ceil(maxY));

    if (iMinX > iMaxX || iMinY > iMaxY) UNLIKELY
        return;

    float area2 = RasterizerCommon::EdgeFunction(v0.Position, v1.Position, v2.Position);
    CullMode cull = state.cullMode;
    if (cull == CullMode::Back  && area2 < 0) return;
    if (cull == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) UNLIKELY return;

    // Triangle edges
    float4 dx01 = v1.Position - v0.Position;
    float4 dx12 = v2.Position - v1.Position;
    float4 dx20 = v0.Position - v2.Position;

    // Broadcast vertex positions for edge function initialization
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
    __m128 dx01v   = _mm_set1_ps(dx01.x);
    __m128 dy01v   = _mm_set1_ps(dx01.y);
    __m128 dx12v   = _mm_set1_ps(dx12.x);
    __m128 dy12v   = _mm_set1_ps(dx12.y);
    __m128 dx20v   = _mm_set1_ps(dx20.x);
    __m128 dy20v   = _mm_set1_ps(dx20.y);

    // Triangle constants, hoisted out of loops
    __m128 v0w  = _mm_set1_ps(v0.Position.w);
    __m128 v1w  = _mm_set1_ps(v1.Position.w);
    __m128 v2w  = _mm_set1_ps(v2.Position.w);
    __m128 ones = _mm_set1_ps(1.0f);
    __m128 zero = _mm_setzero_ps();

    // Incremental edge functions
    // When x → x+4:  Δf = +4 * dy   (dy = edge delta y)
    // When y → y+1:  Δf = -dx        (dx = edge delta x)
    __m128 f01StepX = _mm_set1_ps( 4.0f * dx01.y);
    __m128 f12StepX = _mm_set1_ps( 4.0f * dx12.y);
    __m128 f20StepX = _mm_set1_ps( 4.0f * dx20.y);
    __m128 f01StepY = _mm_set1_ps(-dx01.x);
    __m128 f12StepY = _mm_set1_ps(-dx12.x);
    __m128 f20StepY = _mm_set1_ps(-dx20.x);

    // SIMD start X aligned to multiple of 4
    const int simdStartX = (iMinX / 4) * 4;

    // Initialize edge functions for the first row
    __m128 f01Row, f12Row, f20Row;
    {
        __m128 initX = _mm_set_ps(
            simdStartX + 3.5f, simdStartX + 2.5f,
            simdStartX + 1.5f, simdStartX + 0.5f);
        __m128 initY = _mm_set1_ps(iMinY + 0.5f);

        f01Row = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(initX, v0x), dy01v),
                            _mm_mul_ps(_mm_sub_ps(initY, v0y), dx01v));
        f12Row = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(initX, v1x), dy12v),
                            _mm_mul_ps(_mm_sub_ps(initY, v1y), dx12v));
        f20Row = _mm_sub_ps(_mm_mul_ps(_mm_sub_ps(initX, v2x), dy20v),
                            _mm_mul_ps(_mm_sub_ps(initY, v2y), dx20v));
    }

    uint width = renderTarget.Width();
    for (uint y = iMinY; y <= iMaxY; ++y)
    {
        uint x;
        __m128 f01, f12, f20;

        // Increment f in the for expression; continue does not break accumulation
        for (x = simdStartX, f01 = f01Row, f12 = f12Row, f20 = f20Row;
             x + 3u < width && x <= iMaxX - 3u;
             x += 4,
             f01 = _mm_add_ps(f01, f01StepX),
             f12 = _mm_add_ps(f12, f12StepX),
             f20 = _mm_add_ps(f20, f20StepX))
        {
            if (x > (uint)tileMax.x || x + 3u < (uint)tileMin.x)
                continue;

            // f01/f12/f20 already computed incrementally — 3 adds instead of 6 mul + 6 sub
            __m128 inside;
            if (area2 > 0)
                inside = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(f01, zero),
                                               _mm_cmpge_ps(f12, zero)),
                                               _mm_cmpge_ps(f20, zero));
            else
                inside = _mm_and_ps(_mm_and_ps(_mm_cmple_ps(f01, zero),
                                               _mm_cmple_ps(f12, zero)),
                                               _mm_cmple_ps(f20, zero));

            __m128 alpha = _mm_mul_ps(f12, invArea);
            __m128 beta  = _mm_mul_ps(f20, invArea);
            __m128 gamma = _mm_mul_ps(f01, invArea);

            // Perspective-correct weights
            __m128 pw0 = _mm_mul_ps(alpha, v0w);
            __m128 pw1 = _mm_mul_ps(beta,  v1w);
            __m128 pw2 = _mm_mul_ps(gamma, v2w);

            __m128 pwSum    = _mm_add_ps(_mm_add_ps(pw0, pw1), pw2);
            __m128 invPwSum = _mm_div_ps(ones, pwSum);

            // z: linear interpolation (perspective correction not needed)
            __m128 z = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0z),
                                             _mm_mul_ps(beta,  v1z)),
                                             _mm_mul_ps(gamma, v2z));

            // Attributes: perspective-correct
#define PLERP128(a0, a1, a2) \
    _mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(pw0, a0), \
                                     _mm_mul_ps(pw1, a1)), \
                                     _mm_mul_ps(pw2, a2)), invPwSum)

            __m128 r  = PLERP128(v0cr, v1cr, v2cr);
            __m128 g  = PLERP128(v0cg, v1cg, v2cg);
            __m128 b  = PLERP128(v0cb, v1cb, v2cb);
            __m128 a  = PLERP128(v0ca, v1ca, v2ca);
            __m128 nx = PLERP128(v0nx, v1nx, v2nx);
            __m128 ny = PLERP128(v0ny, v1ny, v2ny);
            __m128 nz = PLERP128(v0nz, v1nz, v2nz);
            __m128 u  = PLERP128(v0u,  v1u,  v2u);
            __m128 v  = PLERP128(v0v,  v1v,  v2v);

#undef PLERP128

            // Depth read via block API
            __m128 depths = depthBuffer.Read4(uint2(x, y));

            __m128 depthCmp;
            switch (state.depthFunc)
            {
            case ComparisonFunc::Never:        depthCmp = _mm_setzero_ps(); break;
            case ComparisonFunc::Less:         depthCmp = _mm_cmplt_ps(z, depths); break;
            case ComparisonFunc::Equal:        depthCmp = _mm_cmpeq_ps(z, depths); break;
            case ComparisonFunc::LessEqual:    depthCmp = _mm_cmple_ps(z, depths); break;
            case ComparisonFunc::Greater:      depthCmp = _mm_cmpgt_ps(z, depths); break;
            case ComparisonFunc::NotEqual:     depthCmp = _mm_cmpneq_ps(z, depths); break;
            case ComparisonFunc::GreaterEqual: depthCmp = _mm_cmpge_ps(z, depths); break;
            case ComparisonFunc::Always:       depthCmp = _mm_castsi128_ps(_mm_set1_epi32(-1)); break;
            default:                           depthCmp = _mm_cmplt_ps(z, depths); break;
            }

            // inside is already a SIMD mask
            __m128 finalMask = _mm_and_ps(depthCmp, inside);
            int depthMask    = _mm_movemask_ps(finalMask);
            if (depthMask == 0)
                continue;

            // Depth write under mask
            depthBuffer.Write4(uint2(x, y), z, finalMask);

            // Scalar loop for shading only
            alignas(16) float zArr[4], rArr[4], gArr[4], bArr[4], aArr[4];
            alignas(16) float uArr[4], vArr[4], nxArr[4], nyArr[4], nzArr[4];
            _mm_store_ps(zArr,  z);
            _mm_store_ps(rArr,  r);  _mm_store_ps(gArr,  g);
            _mm_store_ps(bArr,  b);  _mm_store_ps(aArr,  a);
            _mm_store_ps(uArr,  u);  _mm_store_ps(vArr,  v);
            _mm_store_ps(nxArr, nx); _mm_store_ps(nyArr, ny);
            _mm_store_ps(nzArr, nz);

            for (int i = 0; i < 4; ++i)
            {
                if (!(depthMask & (1 << i))) continue;
                uint px = x + i;
                if (px < (uint)tileMin.x || px > (uint)tileMax.x) continue;

                VertexOutput frag;
                frag.Position = float4((float)px, (float)y, zArr[i], 1.0f);
                frag.Color    = float4(rArr[i], gArr[i], bArr[i], aArr[i]);
                frag.Normal   = float3(nxArr[i], nyArr[i], nzArr[i]);
                frag.UV       = float2(uArr[i], vArr[i]);
                renderTarget.SetPixel(uint2(px, y), ps(frag, cb, *tt));
            }
        }

        // Step to next row
        f01Row = _mm_add_ps(f01Row, f01StepY);
        f12Row = _mm_add_ps(f12Row, f12StepY);
        f20Row = _mm_add_ps(f20Row, f20StepY);

        // Scalar fallback for remaining pixels
        for (; x <= iMaxX; ++x)
        {
            if (x < (uint)tileMin.x || x > (uint)tileMax.x)
                continue;

            float2 p((float)x + 0.5f, (float)y + 0.5f);
            float f0 = RasterizerCommon::EdgeFunction(v1.Position, v2.Position, p);
            float f1 = RasterizerCommon::EdgeFunction(v2.Position, v0.Position, p);
            float f2 = RasterizerCommon::EdgeFunction(v0.Position, v1.Position, p);

            if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0)
                continue;

            float a = f0 / area2;
            float b = f1 / area2;
            float c = f2 / area2;

            VertexOutput frag = RasterizerCommon::Trilerp(v0, v1, v2, a, b, c);
            uint idx = y * width + x;

            bool depthPass = false;
            switch (state.depthFunc)
            {
            case ComparisonFunc::Never:        depthPass = false; break;
            case ComparisonFunc::Less:         depthPass = frag.Position.z <  depthBuffer.At(idx); break;
            case ComparisonFunc::Equal:        depthPass = frag.Position.z == depthBuffer.At(idx); break;
            case ComparisonFunc::LessEqual:    depthPass = frag.Position.z <= depthBuffer.At(idx); break;
            case ComparisonFunc::Greater:      depthPass = frag.Position.z >  depthBuffer.At(idx); break;
            case ComparisonFunc::NotEqual:     depthPass = frag.Position.z != depthBuffer.At(idx); break;
            case ComparisonFunc::GreaterEqual: depthPass = frag.Position.z >= depthBuffer.At(idx); break;
            case ComparisonFunc::Always:       depthPass = true; break;
            }
            if (depthPass)
            {
                depthBuffer.At(idx) = frag.Position.z;
                renderTarget.SetPixel(uint2(x, y), ps(frag, cb, *tt));
            }
        }
    }
}

SOFTX_END