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

namespace QueryRasterizer
{

static inline bool ProcessPixel(uint x, uint y,
                                float fa, float fb, float fc,
                                const Interpolant& v0, const Interpolant& v1, const Interpolant& v2,
                                DepthBuffer& depthBuffer,
                                const Viewport& vp,
                                const RasterizerState& state,
                                uint width)
{
    Interpolant frag = RasterizerCommon::TrilerpDepthOnly(v0, v1, v2, fa, fb, fc);
    float depth = RasterizerCommon::ComputeDepth(frag.Position.z, frag.Position.w, vp);

    float depthValue = depthBuffer.At(int2(x, y));
    bool depthPass = RasterizerCommon::DepthTest(depth, depthValue, state.depthFunc);

    if (depthPass)
    {
        if (state.depthWriteEnable)
            depthBuffer.At(int2(x, y)) = depth;
        return true;
    }
    return false;
}

static inline uint RasterizeTriangle(const RasterizerCommon::TriangleSetup& setup,
                                     const RasterizerState& state,
                                     DepthBuffer& depthBuffer,
                                     const Viewport& vp,
                                     uint2 tileMin,
                                     uint2 tileMax)
{
    uint width = depthBuffer.Width();
    uint visibleCount = 0;

    RasterizerCommon::RasterizeTriangleImpl(setup, tileMin, tileMax, width,
        [&](uint x, uint y, float fa, float fb, float fc) 
        {
            if(ProcessPixel(x, y,
                            fa, fb, fc,
                            setup.v0, setup.v1, setup.v2,
                            depthBuffer,
                            vp,
                            state, 
                            width))
            visibleCount++;
        });
    return visibleCount;
}

} // namespace QueryRasterizer

SOFTX_END
/////////////////////////////////////////////////////////////////
