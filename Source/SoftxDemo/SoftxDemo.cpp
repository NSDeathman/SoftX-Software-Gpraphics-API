#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>
#include <cmath>

#include <SoftX/SoftX.h>
#pragma comment(lib, "SoftX.lib")

using namespace SoftX;
using namespace AfterMath;

// ============================================================
//  Constants
// ============================================================
static constexpr int WINDOW_WIDTH  = 1280;
static constexpr int WINDOW_HEIGHT = 720;

static constexpr int CHECKER_SIZE  = 1024;
static constexpr int CHECKER_CELLS = 16;

// ============================================================
//  Globals
// ============================================================
static HWND    g_hWnd   = nullptr;
static Device* g_device = nullptr;

// ============================================================
//  Constant buffer
// ============================================================
struct ConstantBufferData
{
    float4x4 modelViewProjection;
};

// ============================================================
//  UV checker texture
// ============================================================
static const float4 CHECKER_PALETTE[4] =
{
    float4(0.90f, 0.20f, 0.20f, 1.0f),
    float4(0.20f, 0.60f, 0.90f, 1.0f),
    float4(0.20f, 0.80f, 0.30f, 1.0f),
    float4(0.95f, 0.80f, 0.10f, 1.0f),
};

TextureRGBA32F CreateUVCheckerTexture()
{
    TextureRGBA32F tex(uint2(CHECKER_SIZE, CHECKER_SIZE));
    const int cellSize = CHECKER_SIZE / CHECKER_CELLS;

    for (int y = 0; y < CHECKER_SIZE; ++y)
    {
        for (int x = 0; x < CHECKER_SIZE; ++x)
        {
            int   cellX = x / cellSize;
            int   cellY = y / cellSize;
            float fx    = float(x % cellSize) / float(cellSize);
            float fy    = float(y % cellSize) / float(cellSize);

            const float gridWidth = 0.08f;
            bool isGrid = (fx < gridWidth || fx > 1.0f - gridWidth ||
                           fy < gridWidth || fy > 1.0f - gridWidth);

            float4 color;
            if (isGrid)
            {
                color = float4(1.0f, 1.0f, 1.0f, 1.0f);
            }
            else
            {
                int   paletteIdx = (cellX + cellY) % 4;
                float shade      = 0.75f + 0.25f * (fx * 0.5f + fy * 0.5f);
                color   = CHECKER_PALETTE[paletteIdx];
                color.x *= shade;
                color.y *= shade;
                color.z *= shade;
            }

            __m128 c = _mm_set_ps(color.w, color.z, color.y, color.x);
            tex.StreamWrite(uint2(x, y), c);
        }
    }
    return tex;
}

// ============================================================
//  Shaders
// ============================================================
VertexOutput TransformVS(const VertexInput& input, const ConstantBuffer& cb, const TextureTable& tex)
{
    const ConstantBufferData* data =
        reinterpret_cast<const ConstantBufferData*>(cb.Data());

    VertexOutput output;
    output.Position = float4(input.Position.x, input.Position.y, input.Position.z, 1.0f)
                      * data->modelViewProjection;
    output.Color  = input.Color;
    output.Normal = input.Normal;
    output.UV     = input.UV;
    return output;
}

float4 CheckerPS(const VertexOutput& input, const ConstantBuffer& cb, const TextureTable& tex)
{
    return tex[0].Sample(input.UV);
}

// ============================================================
//  Sphere geometry
// ============================================================
void CreateSphere(VertexBuffer& vb, IndexBuffer& ib,
                  float radius, int slices, int stacks)
{
    std::vector<VertexInput> vertices;
    std::vector<uint>        indices;

    for (int stack = 0; stack <= stacks; ++stack)
    {
        float phi    = PI * float(stack) / float(stacks);
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);

        for (int slice = 0; slice <= slices; ++slice)
        {
            float theta    = 2.0f * PI * float(slice) / float(slices);
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            float3 pos(radius * sinPhi * cosTheta,
                       radius * cosPhi,
                       radius * sinPhi * sinTheta);
            float3 norm = normalize(pos);
            float4 color((norm.x + 1.0f) * 0.5f,
                         (norm.y + 1.0f) * 0.5f,
                         (norm.z + 1.0f) * 0.5f, 1.0f);
            float2 uv(float(slice) / float(slices),
                      float(stack) / float(stacks));

            vertices.push_back({pos, norm, color, uv});
        }
    }

    for (int stack = 0; stack < stacks; ++stack)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            int first  = stack * (slices + 1) + slice;
            int second = first + 1;
            int third  = first + (slices + 1);
            int fourth = third + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(third);

            indices.push_back(second);
            indices.push_back(fourth);
            indices.push_back(third);
        }
    }

    vb = VertexBuffer(std::move(vertices));
    ib = IndexBuffer(std::move(indices));
}

// ============================================================
//  Per-frame rendering
// ============================================================
void DrawFrame()
{
    PROFILE_SCOPE("DrawFrame");

    // ── FPS counter ──────────────────────────────────────────
    static UINT64 frameCount = 0;
    static UINT64 lastUpdateTime = GetTickCount64();
    ++frameCount;

    UINT64 now = GetTickCount64();
    UINT64 elapsed = now - lastUpdateTime;

    if (elapsed >= 1000)
    {
        // Frames rendered in the last elapsed milliseconds
        double fps = double(frameCount) * 1000.0 / double(elapsed);

        char title[128];
        sprintf_s(title, "SoftX — UV Checker Sphere | FPS: %.1f", fps);
        SetWindowTextA(g_hWnd, title);

        frameCount = 0;
        lastUpdateTime = now;
    }

    // ── Build MVP matrix ─────────────────────────────────────
    const float aspect = float(WINDOW_WIDTH) / float(WINDOW_HEIGHT);
    float4x4 projection = perspective(Constants::degrees_to_radians(60.0f), aspect);

    float3 eye(0.0f, 0.0f, -6.0f);
    float3 target(0.0f, 0.0f, 0.0f);
    float4x4 view = look_at(eye, target);

    static float angle = 0.0f;
    angle += 0.01f;

    float4x4 model = rotation_y(angle);
    float4x4 mvp = model * view * projection;

    ConstantBufferData cbData;
    cbData.modelViewProjection = mvp;
    ConstantBuffer mvpCB(&cbData, sizeof(cbData));

    DeviceContext& ctx = g_device->GetImmediateContext();

    // ── Clear ────────────────────────────────────────────────
    ctx.Clear(float4(0.05f, 0.10f, 0.18f, 1.0f));
    ctx.ClearDepth(1.0f);

    // ── Draw sphere ──────────────────────────────────────────
    ctx.SetConstantBuffer(mvpCB);
    ctx.DrawIndexed();

    g_device->Present();
}

// ============================================================
//  Window procedure
// ============================================================
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
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ============================================================
//  Entry point
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    // Register window class
    WNDCLASSEX wc    = {};
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"SoftXDemoWindow";
    RegisterClassEx(&wc);

    // Create window
    RECT rc = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(
        0, L"SoftXDemoWindow", L"SoftX — UV Checker Sphere",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd)
        return -1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Create checker texture (lives for the entire program lifetime)
    TextureRGBA32F checkerTexture = CreateUVCheckerTexture();

    // Create device
    PresentParameters params;
    params.BackBufferSize = uint2(WINDOW_WIDTH, WINDOW_HEIGHT);
    params.hDeviceWindow  = g_hWnd;
    params.Windowed       = true;

    Device device(params);
    g_device = &device;

    // Configure context
    DeviceContext& ctx = g_device->GetImmediateContext();
    ctx.SetRenderTarget(&device.GetBackBuffer(), true);
    ctx.SetViewport(Viewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f));
    ctx.SetTexture(0, &checkerTexture,
                   SamplerState{Filter::Bilinear, Wrap::Repeat, Wrap::Repeat});

    // Build sphere geometry
    VertexBuffer vb;
    IndexBuffer  ib;
    CreateSphere(vb, ib, 3.0f, 64, 32);

    ctx.SetVertexBuffer(vb);
    ctx.SetIndexBuffer(ib);
    ctx.SetVertexShader(TransformVS);
    ctx.SetPixelShader(CheckerPS);
    ctx.SetCullMode(CullMode::Back);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetTileSize(64);
    ctx.SetFillMode(FillMode::Solid);

    // Message loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            PROFILE_FRAME("SoftX");
            DrawFrame();
        }
    }

    return int(msg.wParam);
}
