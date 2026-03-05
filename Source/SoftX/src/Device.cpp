#include "pch.h"
#include <SoftX/SoftX.h>
#include <atomic>

SOFTX_BEGIN

Device::Device(const PresentParameters& params)
    : m_params(params)
    , m_backBuffer(params.BackBufferSize)
    , m_depthBuffer(params.BackBufferSize)
{
}

void Device::SetDeviceContext(DeviceContext ctx)
{
	m_DeviceContext = std::move(ctx);
}

DeviceContext& Device::GetDeviceContext()
{
	return m_DeviceContext;
}

std::unique_ptr<DeviceContext> Device::CreateDeferredContext()
{
	return std::make_unique<DeviceContext>();
}

void Device::SetVertexBuffer(const VertexBuffer& buffer)
{
	m_DeviceContext.SetVertexBuffer(buffer);
}

void Device::SetIndexBuffer(const IndexBuffer& buffer)
{
	m_DeviceContext.SetIndexBuffer(buffer);
}

void Device::SetConstantBuffer(ConstantBuffer cbuffer)
{
	m_DeviceContext.SetConstantBuffer(cbuffer);
}

Framebuffer& Device::GetBackBuffer()
{
    return m_backBuffer;
}

PresentParameters& Device::GetPresentParams()
{
    return m_params;
}

void Device::Present()
{
	PROFILE_SCOPE("Device::Present");

    HDC hdc = GetDC(m_params.hDeviceWindow);
    if (hdc) {
        RECT clientRect;
        GetClientRect(m_params.hDeviceWindow, &clientRect);
        int2 dstSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
        m_backBuffer.present(hdc, int2(0, 0), dstSize);
        ReleaseDC(m_params.hDeviceWindow, hdc);
    }
}

SOFTX_END
