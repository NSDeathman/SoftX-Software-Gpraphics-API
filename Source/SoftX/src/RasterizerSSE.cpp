#include "pch.h"

#include <SoftX/SoftX.h>
#include "RasterizerCommon.h"
#include <xmmintrin.h>

SOFTX_BEGIN

void RasterizerSSE::RasterizeTriangle(
    const VertexOutput& v0,
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

    int width = renderTarget.width();
    int height = renderTarget.height();

    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

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

    auto psLocal = ps;
    auto cbLocal = cb;

    float4 dx01 = v1.Position - v0.Position;
    float4 dx12 = v2.Position - v1.Position;
    float4 dx20 = v0.Position - v2.Position;

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
	__m128 v1nx = _mm_set1_ps(v1.Normal.x);
	__m128 v1ny = _mm_set1_ps(v1.Normal.y);
	__m128 v2nx = _mm_set1_ps(v2.Normal.x);
	__m128 v2ny = _mm_set1_ps(v2.Normal.y);
	__m128 v0nz = _mm_set1_ps(v0.Normal.z);
	__m128 v1nz = _mm_set1_ps(v1.Normal.z);
	__m128 v2nz = _mm_set1_ps(v2.Normal.z);

    __m128 v0u = _mm_set1_ps(v0.UV.x);
    __m128 v0v = _mm_set1_ps(v0.UV.y);
    __m128 v1u = _mm_set1_ps(v1.UV.x);
    __m128 v1v = _mm_set1_ps(v1.UV.y);
    __m128 v2u = _mm_set1_ps(v2.UV.x);
    __m128 v2v = _mm_set1_ps(v2.UV.y);

    __m128 invArea = _mm_set1_ps(1.0f / area2);
    __m128 dx01v = _mm_set1_ps(dx01.x);
    __m128 dy01v = _mm_set1_ps(dx01.y);
    __m128 dx12v = _mm_set1_ps(dx12.x);
    __m128 dy12v = _mm_set1_ps(dx12.y);
    __m128 dx20v = _mm_set1_ps(dx20.x);
    __m128 dy20v = _mm_set1_ps(dx20.y);

    for (int y = iMinY; y <= iMaxY; ++y)
    {
        __m128 baseY = _mm_set1_ps(y + 0.5f);

        int x;
		for (x = iMinX; x + 3 < width && x <= iMaxX - 3; x += 4)
        {
            // Проверка пересечения блока с тайлом (уже учтено в iMinX/iMaxX, но может быть частичное перекрытие? 
            // На самом деле iMinX и iMaxX уже ограничены тайлом, поэтому блок всегда внутри тайла по x.
            // Однако блок может частично выходить за границы тайла по x, если iMinX не кратен 4? 
            // Но мы уже ограничили iMinX и iMaxX, поэтому блок полностью внутри тайла, если x и x+3 в пределах.
            // Для надёжности оставим проверку:
            if (x > tileMax.x || x + 3 < tileMin.x)
                continue;

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
                inside = _mm_and_ps(_mm_and_ps(_mm_cmpge_ps(f01, zero), _mm_cmpge_ps(f12, zero)),
                                    _mm_cmpge_ps(f20, zero));
            }
            else
            {
                inside = _mm_and_ps(_mm_and_ps(_mm_cmple_ps(f01, zero), _mm_cmple_ps(f12, zero)),
                                    _mm_cmple_ps(f20, zero));
            }
            int insideMask = _mm_movemask_ps(inside);
            if (insideMask == 0) continue;

            __m128 alpha = _mm_mul_ps(f12, invArea);
			__m128 beta = _mm_mul_ps(f20, invArea);
			__m128 gamma = _mm_mul_ps(f01, invArea);

			// ── Perspective-correct веса ──────────────────────────────────
			__m128 v0w = _mm_set1_ps(v0.Position.w);
			__m128 v1w = _mm_set1_ps(v1.Position.w);
			__m128 v2w = _mm_set1_ps(v2.Position.w);

			__m128 pw0 = _mm_mul_ps(alpha, v0w);
			__m128 pw1 = _mm_mul_ps(beta, v1w);
			__m128 pw2 = _mm_mul_ps(gamma, v2w);

			__m128 pwSum = _mm_add_ps(_mm_add_ps(pw0, pw1), pw2);
			__m128 ones = _mm_set1_ps(1.0f);
			__m128 invPwSum = _mm_div_ps(ones, pwSum);

			// ── z: линейная интерполяция ───────────────────────────────────
			__m128 z = _mm_add_ps(_mm_add_ps(_mm_mul_ps(alpha, v0z), _mm_mul_ps(beta, v1z)), _mm_mul_ps(gamma, v2z));

			// ── Атрибуты: perspective-correct ─────────────────────────────
#define PLERP128(a0, a1, a2)                                                                                           \
	_mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(pw0, a0), _mm_mul_ps(pw1, a1)), _mm_mul_ps(pw2, a2)), invPwSum)

			__m128 r = PLERP128(v0cr, v1cr, v2cr);
			__m128 g = PLERP128(v0cg, v1cg, v2cg);
			__m128 b = PLERP128(v0cb, v1cb, v2cb);
			__m128 a = PLERP128(v0ca, v1ca, v2ca);
			__m128 nx = PLERP128(v0nx, v1nx, v2nx);
			__m128 ny = PLERP128(v0ny, v1ny, v2ny);
			__m128 nz = PLERP128(v0nz, v1nz, v2nz);
			__m128 u = PLERP128(v0u, v1u, v2u);
			__m128 v = PLERP128(v0v, v1v, v2v);

#undef PLERP128

            int idx0 = y * width + x;
            __m128 depths = _mm_loadu_ps(&depthBuffer.at(idx0));

            __m128 depthCmp;
			switch (state.depthFunc)
			{
			case ComparisonFunc::Never:
				depthCmp = _mm_setzero_ps();
				break;
			case ComparisonFunc::Less:
				depthCmp = _mm_cmplt_ps(z, depths);
				break;
			case ComparisonFunc::Equal:
				depthCmp = _mm_cmpeq_ps(z, depths);
				break;
			case ComparisonFunc::LessEqual:
				depthCmp = _mm_cmple_ps(z, depths);
				break;
			case ComparisonFunc::Greater:
				depthCmp = _mm_cmpgt_ps(z, depths);
				break;
			case ComparisonFunc::NotEqual:
				depthCmp = _mm_cmpneq_ps(z, depths);
				break;
			case ComparisonFunc::GreaterEqual:
				depthCmp = _mm_cmpge_ps(z, depths);
				break;
			case ComparisonFunc::Always:
				depthCmp = _mm_castsi128_ps(_mm_set1_epi32(-1)); // все единицы
				break;
			}
            int depthMask = _mm_movemask_ps(depthCmp) & insideMask;
            if (depthMask == 0) continue;

            // Сохраняем интерполированные значения в массивы
            float zArr[4], rArr[4], gArr[4], bArr[4], aArr[4], uArr[4], vArr[4];
            float nxArr[4], nyArr[4], nzArr[4];
            _mm_storeu_ps(zArr, z);
            _mm_storeu_ps(rArr, r);
            _mm_storeu_ps(gArr, g);
            _mm_storeu_ps(bArr, b);
            _mm_storeu_ps(aArr, a);
            _mm_storeu_ps(uArr, u);
            _mm_storeu_ps(vArr, v);
            _mm_storeu_ps(nxArr, nx);
            _mm_storeu_ps(nyArr, ny);
            _mm_storeu_ps(nzArr, nz);

            for (int i = 0; i < 4; ++i)
            {
                int px = x + i;
                if (px < tileMin.x || px > tileMax.x)
                    continue;

                int bit = 1 << i;
                if (depthMask & bit)
                {
                    int py = y;
                    int idx = py * width + px;

                    depthBuffer.at(idx) = zArr[i];

                    VertexOutput frag;
                    frag.Position = float4((float)px, (float)py, zArr[i], 1.0f);
                    frag.Color = float4(rArr[i], gArr[i], bArr[i], aArr[i]);
                    frag.Normal = float3(nxArr[i], nyArr[i], nzArr[i]);
                    frag.UV = float2(uArr[i], vArr[i]);

                    float4 finalColor = ps(frag, cb, *tt);
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

            if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0)
                continue;

            float a = f0 / area2;
            float b = f1 / area2;
            float c = f2 / area2;

            VertexOutput frag = RasterizerCommon::trilerp(v0, v1, v2, a, b, c);
            int idx = y * width + x;

            bool depthPass = false;
            switch (state.depthFunc) {
                case ComparisonFunc::Never:         depthPass = false; break;
                case ComparisonFunc::Less:          depthPass = frag.Position.z < depthBuffer.at(idx); break;
                case ComparisonFunc::Equal:         depthPass = frag.Position.z == depthBuffer.at(idx); break;
                case ComparisonFunc::LessEqual:     depthPass = frag.Position.z <= depthBuffer.at(idx); break;
                case ComparisonFunc::Greater:       depthPass = frag.Position.z > depthBuffer.at(idx); break;
                case ComparisonFunc::NotEqual:      depthPass = frag.Position.z != depthBuffer.at(idx); break;
                case ComparisonFunc::GreaterEqual:  depthPass = frag.Position.z >= depthBuffer.at(idx); break;
                case ComparisonFunc::Always:        depthPass = true; break;
            }
            if (depthPass) {
                depthBuffer.at(idx) = frag.Position.z;
                float4 finalColor = ps(frag, cb, *tt);
                renderTarget.set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

SOFTX_END
