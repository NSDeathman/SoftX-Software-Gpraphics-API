#pragma once

#include "LibInternal.h"
#include "RenderTargetInterface.h"
#include "DepthBuffer.h"
#include "Types.h"

SOFTX_BEGIN

struct RasterizerState {
    CullMode cullMode = CullMode::Back;
    FillMode fillMode = FillMode::Solid;
};

class IRasterizer {
public:
    virtual ~IRasterizer() = default;

    virtual void RasterizeTriangle(
        const VertexOutput& v0,
        const VertexOutput& v1,
        const VertexOutput& v2,
        const RasterizerState& state,
        DepthBuffer& depthBuffer,
        IRenderTarget& renderTarget,
        const PixelShader& ps,
        const ConstantBuffer& cb,
        int2 tileMin,
        int2 tileMax
    ) = 0;
};

SOFTX_END
