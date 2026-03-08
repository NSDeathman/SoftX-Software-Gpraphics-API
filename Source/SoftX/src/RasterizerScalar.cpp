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

    // Full triangle bounding box
    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    // Clamp bounding box to tile and screen, staying in unsigned range
    uint iMinX = uint(std::max(double(tileMin.x), double(floor(minX))));
    uint iMaxX = uint(std::min(double(tileMax.x), double(std::ceil(maxX))));
    uint iMinY = uint(std::max(double(tileMin.y), double(std::floor(minY))));
    uint iMaxY = uint(std::min(double(tileMax.y), double(std::ceil(maxY))));

    if (iMinX > iMaxX || iMinY > iMaxY) UNLIKELY
        return;

    float area2 = RasterizerCommon::EdgeFunction(v0.Position, v1.Position, v2.Position);
    CullMode cull = state.cullMode;
    if (cull == CullMode::Back && area2 < 0)
        return;
    if (cull == CullMode::Front && area2 > 0)
        return;
    if (std::abs(area2) < 1e-6f) UNLIKELY
        return;

    uint width = renderTarget.Width();

    for (uint y = iMinY; y <= iMaxY; ++y)
    {
        for (uint x = iMinX; x <= iMaxX; ++x)
        {
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
                case ComparisonFunc::Never:
                    depthPass = false;
                    break;
                case ComparisonFunc::Less:
                    depthPass = frag.Position.z < depthBuffer.At(idx);
                    break;
                case ComparisonFunc::Equal:
                    depthPass = frag.Position.z == depthBuffer.At(idx);
                    break;
                case ComparisonFunc::LessEqual:
                    depthPass = frag.Position.z <= depthBuffer.At(idx);
                    break;
                case ComparisonFunc::Greater:
                    depthPass = frag.Position.z > depthBuffer.At(idx);
                    break;
                case ComparisonFunc::NotEqual:
                    depthPass = frag.Position.z != depthBuffer.At(idx);
                    break;
                case ComparisonFunc::GreaterEqual:
                    depthPass = frag.Position.z >= depthBuffer.At(idx);
                    break;
                case ComparisonFunc::Always:
                    depthPass = true;
                    break;
            }

            if (depthPass)
            {
                depthBuffer.At(idx) = frag.Position.z;
                float4 finalColor = ps(frag, cb, *tt);
                renderTarget.SetPixel(uint2(x, y), finalColor);
            }
        }
    }
}

SOFTX_END