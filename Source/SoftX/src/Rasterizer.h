/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "../include/LibInternal.h"
#include "../include/DepthBuffer.h"
#include "../include/RenderTargetInterface.h"
#include "RasterizerCommon.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

namespace Rasterizer
{

static inline void ShadeSinglePixel(uint x, uint y,
                                    float fa, float fb, float fc,
                                    const Interpolant& v0, const Interpolant& v1, const Interpolant& v2,
                                    DepthBuffer& depthBuffer, IRenderTarget* renderTarget,
                                    const Viewport& vp,
                                    const PixelShader& ps, const ConstantBuffer& cb, const TextureTable* tt,
                                    const RasterizerState& state,
                                    uint width)
{
    Interpolant frag = RasterizerCommon::Trilerp(v0, v1, v2, fa, fb, fc);

    float depth = RasterizerCommon::ComputeDepth(frag.Position.z, frag.Position.w, vp);

    float depthValue = depthBuffer.At(int2(x, y));

    if (!RasterizerCommon::DepthTest(depth, depthValue, state.depthFunc))
        return;

    if (state.depthWriteEnable)
        depthBuffer.At(int2(x, y)) = depth;

    if (renderTarget)
        renderTarget->SetPixel(uint2(x, y), ps(frag, cb, *tt));
}

static inline void RasterizeTriangle(const RasterizerCommon::TriangleSetup& setup,
                                     const RasterizerState& state,
                                     DepthBuffer& depthBuffer,
                                     IRenderTarget* renderTarget,
                                     const Viewport& vp,
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
            ShadeSinglePixel(x, y, 
                             fa, fb, fc,
                             setup.v0, setup.v1, setup.v2,
                             depthBuffer, renderTarget,
                             vp,
                             ps, cb, tt,
                             state, 
                             width);
        });
}

} // namespace Rasterizer

SOFTX_END
/////////////////////////////////////////////////////////////////
