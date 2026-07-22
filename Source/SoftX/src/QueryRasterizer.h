/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "Rasterizer.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

namespace QueryRasterizer
{

static inline bool ProcessPixel(uint x, uint y,
                                float fa, float fb, float fc,
                                const Interpolant& v0, const Interpolant& v1, const Interpolant& v2,
                                DepthBuffer& depthBuffer,
                                const RasterizerState& state,
                                uint width)
{
    float z = fa * v0.Position.z + fb * v1.Position.z + fc * v2.Position.z;

    const uint idx = y * width + x;
    float depthValue = depthBuffer.At(idx);
    bool depthPass = RasterizerCommon::DepthTest(z, depthValue, state.depthFunc);

    if (depthPass)
    {
        if (state.depthWriteEnable)
            depthBuffer.At(idx) = z;
        return true;
    }
    return false;
}

static inline uint RasterizeTriangle(const RasterizerCommon::TriangleSetup& setup,
                                     const RasterizerState& state,
                                     DepthBuffer& depthBuffer,
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
                            state, 
                            width))
            visibleCount++;
        });
    return visibleCount;
}

} // namespace QueryRasterizer

SOFTX_END
/////////////////////////////////////////////////////////////////
