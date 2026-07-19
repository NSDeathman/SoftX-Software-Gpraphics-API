/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <SoftX.h>
#include <AfterMath.h>
/////////////////////////////////////////////////////////////////
using namespace SoftX;
using namespace AfterMath;
/////////////////////////////////////////////////////////////////
Device* g_device = nullptr;
/////////////////////////////////////////////////////////////////
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
            DestroyWindow(hWnd);
        return 0;

    case WM_SIZE:
    {
        if (g_device && wParam != SIZE_MINIMIZED)
        {
            int newWidth = LOWORD(lParam);
            int newHeight = HIWORD(lParam);

            if (newWidth > 0 && newHeight > 0)
            {
                PresentParameters newParams = g_device->GetPresentParams();
                newParams.BackBufferSize = uint2(newWidth, newHeight);
                g_device->Reset(newParams);

                DeviceContext& ctx = g_device->GetImmediateContext();
                ctx.SetViewport(Viewport(0.0f, 0.0f, newWidth, newHeight, 0.0f, 1.0f));
            }
        }
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    uint2 windowSize = uint2(512, 512);

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "Hello Triangle";
    RegisterClassEx(&wc);

    RECT rc = { 0, 0, (LONG)windowSize.x, (LONG)windowSize.y };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND Wnd = CreateWindowEx(0, "Hello Triangle", "SoftX Triangle Demo",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              nullptr, nullptr, hInstance, nullptr);
    if (!Wnd) return -1;

    ShowWindow(Wnd, nCmdShow);
    UpdateWindow(Wnd);

    PresentParameters params;
    params.BackBufferSize = windowSize;
    params.hDeviceWindow = Wnd;
    params.Windowed = true;

    Device device(params);
    g_device = &device;
    DeviceContext& ctx = g_device->GetImmediateContext();

    VertexBuffer vb;
    vb.Reserve(3);
    vb.Add(float3(-1.0f, -1.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f));
    vb.Add(float3(0.0f, 1.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f));
    vb.Add(float3(1.0f, -1.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f));

    auto vs = [](const Vertex& Input, ConstantBuffer, const TextureTable&) -> Interpolant
    {
        Interpolant Output;
        Output.Position = float4(Input.Position, 1.0f);
        Output.Color = Input.Color;
        return Output;
    };
    auto ps = [](const Interpolant& Input, ConstantBuffer, const TextureTable&) -> float4
    {
        return Input.Color;
    };

    ctx.SetRenderTarget(device.GetBackBuffer(), false);
    ctx.SetViewport(Viewport(0.0f, 0.0f, windowSize.x, windowSize.y, 0.0f, 1.0f));
    ctx.SetCullMode(CullMode::None);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetFillMode(FillMode::Solid);
    ctx.SetVertexBuffer(vb);
    ctx.SetVertexShader(vs);
    ctx.SetPixelShader(ps);
    ctx.SetTileSize(64);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ctx.Clear(ClearFlags::All, float4(0.10f, 0.05f, 0.12f, 1.0f), 1.0f);
        ctx.Draw();
        g_device->Present();
    }

	return 0;
}
/////////////////////////////////////////////////////////////////
