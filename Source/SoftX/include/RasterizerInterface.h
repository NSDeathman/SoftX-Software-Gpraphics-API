/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "LibInternal.h"
#include "Types.h"
/////////////////////////////////////////////////////////////////
class SOFTX_API SoftX::IRenderTarget;
class SOFTX_API SoftX::DepthBuffer;
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

struct RasterizerState 
{
    CullMode cullMode = CullMode::Back;
    FillMode fillMode = FillMode::Solid;
	ComparisonFunc depthFunc = ComparisonFunc::Less;
    bool depthWriteEnable = true;
};

class IRasterizer 
{
public:
    virtual ~IRasterizer() = default;

    virtual void RasterizeTriangle(const Interpolant& v0,
                                   const Interpolant& v1,
                                   const Interpolant& v2,
                                   const RasterizerState& state,
                                   DepthBuffer& depthBuffer,
                                   IRenderTarget* renderTarget,
                                   const PixelShader& ps,
                                   const ConstantBuffer& cb,
                                   const TextureTable* tt,
                                   uint2 tileMin,
                                   uint2 tileMax) = 0;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
