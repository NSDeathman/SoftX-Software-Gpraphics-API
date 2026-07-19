/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "../include/QueryRasterizerInterface.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class SOFTX_API QueryRasterizerAVX : public IQueryRasterizer
{
  public:
    uint RasterizeTriangle(const Interpolant& v0,
                           const Interpolant& v1,
                           const Interpolant& v2,
                           const RasterizerState& state,
                           DepthBuffer& depthBuffer,
                           const ConstantBuffer& cb,
                           uint2 tileMin,
                           uint2 tileMax) override;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
