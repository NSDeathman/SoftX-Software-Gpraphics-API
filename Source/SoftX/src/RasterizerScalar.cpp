#include "pch.h"

#include <SoftX/SoftX.h>
#include "RasterizerCommon.h"

SOFTX_BEGIN

void RasterizerScalar::RasterizeTriangle(
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
    PROFILE_SCOPE("RasterizerScalar::RasterizeTriangleTile");

    int width = renderTarget.width();
    int height = renderTarget.height();

    // Полный bounding box треугольника
    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    // Ограничиваем bounding box тайлом и экраном
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

    for (int y = iMinY; y <= iMaxY; ++y)
    {
        for (int x = iMinX; x <= iMaxX; ++x)
        {
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
                float4 finalColor = ps(frag, cb);
                renderTarget.set_pixel(int2(x, y), finalColor);
            }
        }
    }
}

SOFTX_END
