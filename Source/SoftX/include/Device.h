/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include <functional>
#include <windows.h>

#include "DeviceContext.h"
#include "LibInternal.h"
#include "ThirdPartyIncluding.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class SOFTX_API Device
{
public:
    explicit Device(const PresentParameters& params);
    ~Device();

    Device(Device&&) = default;
    Device& operator=(Device&&) = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    Device CreateHeadless(uint2 backBufferSize);

    void SetDeviceContext(DeviceContext ctx);
    DeviceContext& GetDeviceContext();
    const DeviceContext& GetDeviceContext() const;

    DeviceContext& GetImmediateContext()
    {
        return immediateContext;
    }
    const DeviceContext& GetImmediateContext() const
    {
        return immediateContext;
    }

    std::unique_ptr<DeviceContext> CreateDeferredContext();

    void Present();

    Framebuffer& GetBackBuffer();
    const Framebuffer& GetBackBuffer() const;

    PresentParameters& GetPresentParams();
    const PresentParameters& GetPresentParams() const;

private:
    void SetupOutputConsole();
    void DestroyOutputConsole();

    void PresentToWindow();
    void PresentToConsole();

private:
    PresentParameters presentParams;
    Framebuffer backBuffer;
    DepthBuffer depthBuffer;
    DeviceContext immediateContext;
    HANDLE hConsoleBuffer = nullptr;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
