/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "RasterizerCommon.h"
#include "../include/LibInternal.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

namespace Rasterizer
{

// Shared depth test + pixel write — called from both traversal paths.
// Inlined by the compiler; extracted here to avoid duplicating the switch.
static inline void ShadeSinglePixel(uint x, uint y,
                                    float fa, float fb, float fc,
                                    const Interpolant& v0, const Interpolant& v1, const Interpolant& v2,
                                    DepthBuffer& depthBuffer, IRenderTarget* renderTarget,
                                    const PixelShader& ps, const ConstantBuffer& cb, const TextureTable* tt,
                                    const RasterizerState& state,
                                    uint width)
{
    const uint idx = y * width + x;
    float depthValue = depthBuffer.At(idx);
    float zInterp = fa * v0.Position.z + fb * v1.Position.z + fc * v2.Position.z;
    bool depthPass = RasterizerCommon::DepthTest(zInterp, depthValue, state.depthFunc);
    if (!depthPass)
        return;

    Interpolant frag = RasterizerCommon::Trilerp(v0, v1, v2, fa, fb, fc);

    if (state.depthWriteEnable)
        depthBuffer.At(idx) = frag.Position.z;
    if(renderTarget != nullptr)
        renderTarget->SetPixel(uint2(x, y), ps(frag, cb, *tt));
}

static inline void RasterizeTriangle(const RasterizerCommon::TriangleSetup& setup,
                                     const RasterizerState& state,
                                     DepthBuffer& depthBuffer,
                                     IRenderTarget* renderTarget,
                                     const PixelShader& ps,
                                     const ConstantBuffer& cb,
                                     const TextureTable* tt,
                                     uint2 tileMin,
                                     uint2 tileMax)
{
    uint width = renderTarget ? renderTarget->Width() : depthBuffer.Width();

    RasterizerCommon::RasterizeTriangleImpl(setup, tileMin, tileMax, width,
        [&](uint x, uint y, float fa, float fb, float fc)
        {
            ShadeSinglePixel(x, y, fa, fb, fc,
                setup.v0, setup.v1, setup.v2,
                depthBuffer, renderTarget,
                ps, cb, tt, state, width);
        });
}

} // namespace Rasterizer

SOFTX_END
/////////////////////////////////////////////////////////////////
