#pragma once
#include <windows.h>
#include <functional>
#include "Types.h"
#include "Framebuffer.h"
#include "DepthBuffer.h"
#include "RenderTargetTexture.h"

class Device {
public:
    Device(const PresentParameters& params);
    ~Device() = default;

    // Очистка заднего буфера цветом
    void Clear(const float4& color) { m_currentRT->clear(color); };
    void ClearDepth(float depth) { m_depthBuffer.clear(depth); };

    // Установка пиксельного шейдера и константного буфера
    void SetPixelShader(PixelShader shader) { m_pixelShader = std::move(shader); };
    void SetVertexShader(VertexShader shader) { m_vertexShader = std::move(shader); };

    void SetConstantBuffer(const void* ConstantBuffer, size_t size) { m_constant_buffer = ConstantBuffer; m_constant_buffer_size = size; };

    void SetVertexBuffer(const std::vector<VertexInput>& buffer) { m_vertexBuffer = buffer; }
    void SetIndexBuffer(const std::vector<uint32_t>& buffer) { m_indexBuffer = buffer; }

    void SetRenderTarget(IRenderTarget* rt) { m_currentRT = rt ? rt : &m_backBuffer; }
    IRenderTarget* GetRenderTarget() const { return m_currentRT; }

    void SetViewport(const Viewport& vp) { m_viewport = vp; }
    const Viewport& GetViewport() const { return m_viewport; }

    // Рисует полноэкранный четырёхугольник, выполняя пиксельный шейдер для каждого пикселя
	void DrawFullScreenQuad();

    void DrawIndexed(uint32_t indexCount, uint32_t startIndex);

    float4 ClipToScreen(const float4& clipPos) const;
    void RasterizeTriangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2);

    void SetCullMode(CullMode mode) { m_cullMode = mode; }
    CullMode GetCullMode() const { return m_cullMode; }

    // Презентация: копирует задний буфер в окно
    void Present();

    // Доступ к заднему буферу для рисования (прямое манипулирование пикселями)
    Framebuffer& GetBackBuffer() { return m_backBuffer; }
    const Framebuffer& GetBackBuffer() const { return m_backBuffer; }

    // Геттеры для параметров
    const PresentParameters& GetPresentParams() const { return m_params; }

private:
    PresentParameters m_params;

    Framebuffer m_backBuffer;
    DepthBuffer m_depthBuffer;

    IRenderTarget* m_currentRT;

    Viewport m_viewport;

    PixelShader m_pixelShader;
    VertexShader m_vertexShader;

    const void* m_constant_buffer;
    size_t m_constant_buffer_size;

    std::vector<VertexInput> m_vertexBuffer;
    std::vector<uint32_t> m_indexBuffer;

    CullMode m_cullMode;
};
