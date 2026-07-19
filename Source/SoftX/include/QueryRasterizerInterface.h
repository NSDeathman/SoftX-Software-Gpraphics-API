/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "RasterizerInterface.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class IQueryRasterizer 
{
public:
    virtual ~IQueryRasterizer() = default;

    virtual uint RasterizeTriangle(const Interpolant& v0,
                                   const Interpolant& v1,
                                   const Interpolant& v2,
                                   const RasterizerState& state,
                                   DepthBuffer& depthBuffer,
                                   const ConstantBuffer& cb,
                                   uint2 tileMin,
                                   uint2 tileMax) = 0;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
