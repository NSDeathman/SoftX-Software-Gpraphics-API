#pragma once

#include <windows.h>
#include <functional>

#include "ThirdPartyIncluding.h"
#include "LibInternal.h"
#include "ThreadPool.h"
#include "DeviceContext.h"

SOFTX_BEGIN

class SOFTX_API Device {
public:
    Device(const PresentParameters& params);
    ~Device() = default;

	void SetDeviceContext(const DeviceContext ctx);
	DeviceContext& GetDeviceContext();

    DeviceContext& GetImmediateContext() { return m_DeviceContext; }
    const DeviceContext& GetImmediateContext() const { return m_DeviceContext; }

	std::unique_ptr<DeviceContext> CreateDeferredContext();

	void SetVertexBuffer(const VertexBuffer& buffer);
	void SetIndexBuffer(const IndexBuffer& buffer);
	void SetConstantBuffer(ConstantBuffer cbuffer);

    // Презентация: копирует задний буфер в окно
    void Present();

    // Доступ к заднему буферу для рисования (прямое манипулирование пикселями)
    Framebuffer& GetBackBuffer();

    // Геттеры для параметров
    PresentParameters& GetPresentParams();

private:
    PresentParameters m_params;

    Framebuffer m_backBuffer;
    DepthBuffer m_depthBuffer;

	DeviceContext m_DeviceContext;
};

SOFTX_END
