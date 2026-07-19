/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "../include/LibInternal.h"
#include "../include/RasterizerInterface.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class SOFTX_API RasterizerScalar : public IRasterizer
{
  public:
    void RasterizeTriangle(const Interpolant& v0,
                           const Interpolant& v1,
                           const Interpolant& v2,
                           const RasterizerState& state,
                           DepthBuffer& depthBuffer,
                           IRenderTarget* renderTarget,
                           const PixelShader& ps,
                           const ConstantBuffer& cb,
                           const TextureTable* tt,
                           uint2 tileMin,
                           uint2 tileMax) override;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
