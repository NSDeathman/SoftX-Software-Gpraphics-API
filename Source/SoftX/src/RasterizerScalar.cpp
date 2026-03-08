#include "pch.h"

#include "RasterizerCommon.h"
#include <SoftX/SoftX.h>

SOFTX_BEGIN

void RasterizerScalar::RasterizeTriangle(const VertexOutput& v0, 
                                         const VertexOutput& v1, 
                                         const VertexOutput& v2,
                                         const RasterizerState& state, 
                                         DepthBuffer& depthBuffer,
                                         IRenderTarget& renderTarget, 
                                         const PixelShader& ps, 
                                         const ConstantBuffer& cb,
                                         const TextureTable* tt, 
                                         uint2 tileMin, 
                                         uint2 tileMax)
{
    PROFILE_SCOPE("RasterizerScalar::RasterizeTriangle");

    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    uint iMinX = uint(std::max(double(tileMin.x), double(std::floor(minX))));
    uint iMaxX = uint(std::min(double(tileMax.x), double(std::ceil(maxX))));
    uint iMinY = uint(std::max(double(tileMin.y), double(std::floor(minY))));
    uint iMaxY = uint(std::min(double(tileMax.y), double(std::ceil(maxY))));

    if (iMinX > iMaxX || iMinY > iMaxY) UNLIKELY 
        return;

    // ── Fixed-point vertex coordinates (28.4) ───────────────────────────────
    const int x0 = RasterizerCommon::ToFixed(v0.Position.x);
    const int y0 = RasterizerCommon::ToFixed(v0.Position.y);
    const int x1 = RasterizerCommon::ToFixed(v1.Position.x);
    const int y1 = RasterizerCommon::ToFixed(v1.Position.y);
    const int x2 = RasterizerCommon::ToFixed(v2.Position.x);
    const int y2 = RasterizerCommon::ToFixed(v2.Position.y);

    // Signed area in fixed-point² units (same sign as float area2)
    int64_t area2 = RasterizerCommon::EdgeFunctionInt(x0, y0, x1, y1, x2, y2);

    const CullMode cull = state.cullMode;
    if (cull == CullMode::Back  && area2 < 0) 
        return;
    if (cull == CullMode::Front && area2 > 0) 
        return;
    if (area2 == 0) UNLIKELY 
        return;

    // Normalise to CCW so the inside test is always  f >= 0.
    // Negating f and area2 together preserves all ratios f/area2,
    // so barycentric coordinates are unchanged.
    if (area2 < 0)
    {
        area2 = -area2;
    }
    const int sign = (RasterizerCommon::EdgeFunctionInt(x0, y0, x1, y1, x2, y2) > 0) ? 1 : -1;

    // Per-pixel X-step:   ΔE(x→x+1) =  S·Δy_fp
    // Per-row  Y-step:    ΔE(y→y+1) = −S·Δx_fp
    const int stepX01 = sign *  RasterizerCommon::SUBPIXEL_STEP * (y1 - y0);
    const int stepX12 = sign *  RasterizerCommon::SUBPIXEL_STEP * (y2 - y1);
    const int stepX20 = sign *  RasterizerCommon::SUBPIXEL_STEP * (y0 - y2);
    const int stepY01 = sign * -RasterizerCommon::SUBPIXEL_STEP * (x1 - x0);
    const int stepY12 = sign * -RasterizerCommon::SUBPIXEL_STEP * (x2 - x1);
    const int stepY20 = sign * -RasterizerCommon::SUBPIXEL_STEP * (x0 - x2);

    // Edge functions at first pixel centre (iMinX, iMinY)
    const int px0 = RasterizerCommon::PixelCentre(iMinX);
    const int py0 = RasterizerCommon::PixelCentre(iMinY);

    int64_t f01Row = sign * RasterizerCommon::EdgeFunctionInt(x0, y0, x1, y1, px0, py0);
    int64_t f12Row = sign * RasterizerCommon::EdgeFunctionInt(x1, y1, x2, y2, px0, py0);
    int64_t f20Row = sign * RasterizerCommon::EdgeFunctionInt(x2, y2, x0, y0, px0, py0);

    // invArea2: ratio f_int/area2_int == f_float/area2_float (S² cancels out)
    const float invArea2 = 1.0f / float(area2);
    const uint  width    = renderTarget.Width();

    for (uint y = iMinY; y <= iMaxY; ++y, f01Row += stepY01, f12Row += stepY12, f20Row += stepY20)
    {
        int64_t f01 = f01Row;
        int64_t f12 = f12Row;
        int64_t f20 = f20Row;

        for (uint x = iMinX; x <= iMaxX; ++x, f01 += stepX01, f12 += stepX12, f20 += stepX20)
        {
            // Single branch: OR of sign bits — negative if any f < 0
            if ((f01 | f12 | f20) < 0) continue;

            const float a = float(f12) * invArea2; // weight for v0
            const float b = float(f20) * invArea2; // weight for v1
            const float c = float(f01) * invArea2; // weight for v2

            VertexOutput frag = RasterizerCommon::Trilerp(v0, v1, v2, a, b, c);
            const uint idx    = y * width + x;

            bool depthPass = false;
            switch (state.depthFunc)
            {
                case ComparisonFunc::Never:        depthPass = false;                                   break;
                case ComparisonFunc::Less:         depthPass = frag.Position.z <  depthBuffer.At(idx); break;
                case ComparisonFunc::Equal:        depthPass = frag.Position.z == depthBuffer.At(idx); break;
                case ComparisonFunc::LessEqual:    depthPass = frag.Position.z <= depthBuffer.At(idx); break;
                case ComparisonFunc::Greater:      depthPass = frag.Position.z >  depthBuffer.At(idx); break;
                case ComparisonFunc::NotEqual:     depthPass = frag.Position.z != depthBuffer.At(idx); break;
                case ComparisonFunc::GreaterEqual: depthPass = frag.Position.z >= depthBuffer.At(idx); break;
                case ComparisonFunc::Always:       depthPass = true;                                   break;
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