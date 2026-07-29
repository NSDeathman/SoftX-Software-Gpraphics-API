/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <chrono>

#include <SoftX.h>
#include <AfterMath.h>
/////////////////////////////////////////////////////////////////
using namespace SoftX;
using namespace AfterMath;
/////////////////////////////////////////////////////////////////
Device* g_device = nullptr;
uint2 g_windowSize = uint2(512, 512);
/////////////////////////////////////////////////////////////////
struct CbData
{
    float4x4 modelViewProjection;
};

void CreateCube(VertexBuffer& vb, IndexBuffer& ib, const float4& color = float4(0.0f, 0.0f, 0.0f, 0.0f))
{
    //     7-------6
    //    /|      /|
    //   3-------2  |
    //   |  4----|--5     y
    //   | /     | /      |
    //   0-------1        +--x
    //                   /
    //                  z
    const float3 positions[8] = {
        {-0.5f, -0.5f, -0.5f}, // 0
        { 0.5f, -0.5f, -0.5f}, // 1
        { 0.5f,  0.5f, -0.5f}, // 2
        {-0.5f,  0.5f, -0.5f}, // 3
        {-0.5f, -0.5f,  0.5f}, // 4
        { 0.5f, -0.5f,  0.5f}, // 5
        { 0.5f,  0.5f,  0.5f}, // 6
        {-0.5f,  0.5f,  0.5f}  // 7
    };

    struct Face { int i[4]; float3 normal; };
    const Face faces[6] =
    {
        {{1, 5, 6, 2}, float3(1,0,0)},  // +X
        {{4, 0, 3, 7}, float3(-1,0,0)}, // -X
        {{2, 6, 7, 3}, float3(0,1,0)},  // +Y
        {{4, 5, 1, 0}, float3(0,-1,0)}, // -Y
        {{5, 4, 7, 6}, float3(0,0,1)},  // +Z
        {{0, 1, 2, 3}, float3(0,0,-1)}  // -Z
    };

    vb.Reserve(24);
    ib.Reserve(36);

    for (const auto& face : faces)
    {
        uint base = vb.Size();
        for (int idx : face.i)
        {
            vb.Add(positions[idx], color, face.normal);
        }

        // Two triangles: 0-1-2 and 2-3-0 (CCW)
        ib.AddTri(base + 0, base + 1, base + 2);
        ib.AddTri(base + 2, base + 3, base + 0);
    }
}

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
                g_windowSize = uint2(newWidth, newHeight);
                newParams.BackBufferSize = g_windowSize;
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
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = "Hello Triangle";
    RegisterClassEx(&wc);

    RECT rc = { 0, 0, (LONG)g_windowSize.x, (LONG)g_windowSize.y };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND Wnd = CreateWindowEx(0, "Hello Triangle", "SoftX Triangle Demo",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              nullptr, nullptr, hInstance, nullptr);
    if (!Wnd) return -1;

    ShowWindow(Wnd, nCmdShow);
    UpdateWindow(Wnd);

    PresentParameters params;
    params.BackBufferSize = g_windowSize;
    params.hDeviceWindow = Wnd;
    params.Windowed = true;

    Device device(params);
    g_device = &device;
    DeviceContext& ctx = g_device->GetImmediateContext();

    VertexBuffer vb;
    IndexBuffer ib;
    CreateCube(vb, ib, float4(0.1f, 0.6f, 0.1f, 1.0f));

    auto vs = [](const Vertex& Input, ConstantBuffer cb, const TextureTable&) -> Interpolant
    {
        const CbData* CBuf = reinterpret_cast<const CbData*>(cb.Data());
        Interpolant Output;
        Output.Position = float4(Input.Position, 1.0f) * CBuf->modelViewProjection;
        Output.Color = Input.Color;
        return Output;
    };
    auto ps = [](const Interpolant& Input, ConstantBuffer, const TextureTable&) -> float4
    {
        return Input.Color;
    };

    ctx.SetRenderTarget(device.GetBackBuffer(), false);
    ctx.SetViewport(Viewport(0.0f, 0.0f, g_windowSize.x, g_windowSize.y, 0.0f, 1.0f));
    ctx.SetCullMode(CullMode::None);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetFillMode(FillMode::Solid);
    ctx.SetVertexBuffer(vb);
    ctx.SetIndexBuffer(ib);
    ctx.SetVertexShader(vs);
    ctx.SetPixelShader(ps);
    ctx.SetTileSize(128);

    auto lastTime = std::chrono::steady_clock::now();
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        const float aspect = float(g_windowSize.x) / float(g_windowSize.y);
        float4x4 proj = perspective(Constants::degrees_to_radians(60.0f), aspect);
        float3 eye(0.0f, 0.0f, -2.0f);
        float3 target(0.0f, 0.0f, 0.0f);
        float4x4 view = look_at(eye, target);

        float3 cubePos = float3(0.0f, 0.0f, 0.0f);
        static float cubeRotation = 0.0f;
        cubeRotation += 0.75f * deltaTime;
        float4x4 cubeWorld = rotation_y(cubeRotation) * translation(cubePos) * scaling(float3(1.0f, 1.0f, 1.0f));
        float4x4 mvp = cubeWorld * view * proj;
        CbData cData;
        cData.modelViewProjection = mvp;
        ConstantBuffer cbuf(&cData, sizeof(cData));

        ctx.Clear(ClearFlags::All, float4(0.10f, 0.05f, 0.12f, 1.0f), 1.0f);
        ctx.SetConstantBuffer(cbuf);
        ctx.DrawIndexed();
        g_device->Present();
    }

	return 0;
}
/////////////////////////////////////////////////////////////////
