/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include "../include/SoftX.h"
#include "RasterizerFactory.h"
#include "ThreadPoolManager.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

DeviceContext::DeviceContext() : stateMutex(std::make_unique<std::mutex>()), 
                                 drawMutex(std::make_unique<std::mutex>())
{
    rasterizer = CreateBestRasterizer();
}

DeviceContext::DeviceContext(const PipelineStateObject& initialState) : backState(initialState), 
                                                                        frontState(initialState),
                                                                        stateMutex(std::make_unique<std::mutex>()),
                                                                        drawMutex(std::make_unique<std::mutex>())
{
    rasterizer = CreateBestRasterizer();
}

DeviceContext::~DeviceContext() = default;

void DeviceContext::SetVertexShader(VertexShader shader) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.vertexShader = std::move(shader);
}

void DeviceContext::SetGeometryShader(GeometryShader shader)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.geometryShader = std::move(shader);
}

void DeviceContext::SetPixelShader(PixelShader shader) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.pixelShader = std::move(shader);
}

void DeviceContext::SetIndexBuffer(const IndexBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.indexBuffer = buffer;
}

void DeviceContext::SetVertexBuffer(const VertexBuffer& buffer) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.vertexBuffer = buffer;
}

void DeviceContext::SetTexture(const std::string& name,
                               std::shared_ptr<const ITexture> texture,
                               SamplerState sampler) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.textureTable.Set(name, std::move(texture), sampler);
}

void DeviceContext::SetRenderTarget(std::shared_ptr<IRenderTarget> target, bool createDepthBuffer) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.renderTarget = std::move(target);
    if (createDepthBuffer && backState.renderTarget) 
    {
        uint2 size = backState.renderTarget->Size();
        if (!backState.depthBuffer || backState.depthBuffer->Size() != size)
            backState.depthBuffer = std::make_shared<DepthBuffer>(size);
    }
}

void DeviceContext::SetDepthBuffer(std::shared_ptr<DepthBuffer> depth) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.depthBuffer = std::move(depth);
}

void DeviceContext::SetViewport(const Viewport& vp) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.viewport = vp;
}

void DeviceContext::SetTileSize(uint size) 
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.tileSize = size;
}

void DeviceContext::SetDepthWriteEnable(bool enable)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.depthWriteEnable = enable;
}

void DeviceContext::SetCullMode(CullMode mode)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.cullMode = mode;
}

void DeviceContext::SetFillMode(FillMode mode)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.fillMode = mode;
}

void DeviceContext::SetDepthFunc(ComparisonFunc func)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.depthFunc = func;
}

void DeviceContext::SetConstantBuffer(const ConstantBuffer& buffer)
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    backState.constantBuffer = buffer;
}

void DeviceContext::CommitState()
{
    std::lock_guard<std::mutex> lock(*stateMutex);
    frontState = backState;
}

PipelineStateObject DeviceContext::CaptureState() const
{
    return frontState;
}

void DeviceContext::Clear(const float4& color) 
{
    std::lock_guard<std::mutex> lock(*drawMutex);
    PROFILE_SCOPE("DeviceContext::Clear");
    CommitState();
    PipelineStateObject state = frontState;
    if (state.renderTarget)
        state.renderTarget->Clear(color);
}

void DeviceContext::ClearDepth(const float& depth) 
{
    std::lock_guard<std::mutex> lock(*drawMutex);
    PROFILE_SCOPE("DeviceContext::ClearDepth");
    CommitState();
    PipelineStateObject state = frontState;
    if (state.depthBuffer)
        state.depthBuffer->Clear(depth);
}

void DeviceContext::ClearColorAndDepth(const float4& color, const float& depth) 
{
    std::lock_guard<std::mutex> lock(*drawMutex);
    PROFILE_SCOPE("DeviceContext::ClearColorAndDepth");
    CommitState();
    PipelineStateObject state = frontState;
    auto& pool = ThreadPoolManager::Get();
    if (state.renderTarget && state.depthBuffer && pool.threadCount() > 0) 
    {
        pool.enqueue([rt = state.renderTarget, color]{ rt->Clear(color); });
        pool.enqueue([db = state.depthBuffer, depth]{ db->Clear(depth); });
        pool.wait();
    }
    else 
    {
        if (state.renderTarget) state.renderTarget->Clear(color);
        if (state.depthBuffer)  state.depthBuffer->Clear(depth);
    }
}

SOFTX_END
/////////////////////////////////////////////////////////////////
