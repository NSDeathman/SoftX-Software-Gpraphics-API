#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>
#include <cmath>

#include <SoftX/SoftX.h>

using namespace SoftX;
using namespace AfterMath;

// ============================================================
//  Constants
// ============================================================
static constexpr int WINDOW_WIDTH = 1280;
static constexpr int WINDOW_HEIGHT = 720;

// ============================================================
//  Globals
// ============================================================
static HWND    g_hWnd = nullptr;
static Device* g_device = nullptr;

// Ресурсы для depth‑only прохода
static std::unique_ptr<DepthBuffer>      g_depthOnly; // буфер глубины
static std::unique_ptr<DepthTextureView> g_depthView; // представление буфера как текстуры

// ============================================================
//  Constant buffer
// ============================================================
struct ConstantBufferData
{
    float4x4 modelViewProjection;
};

// ============================================================
//  Shaders
// ============================================================
VertexOutput TransformVS(const VertexInput& input, const ConstantBuffer& cb, const TextureTable& tex)
{
    const ConstantBufferData* data = reinterpret_cast<const ConstantBufferData*>(cb.Data());
    VertexOutput output;
    output.Position = float4(input.Position.x, input.Position.y, input.Position.z, 1.0f)
        * data->modelViewProjection;
    output.Color = input.Color;
    output.Normal = input.Normal;
    output.UV = input.UV;
    return output;
}

// Пиксельный шейдер визуализации глубины:
// сэмплирует текстуру глубины (через ITexture) и показывает инвертированную глубину
float4 DepthVisualizePS(const VertexOutput& input, const ConstantBuffer& cb, const TextureTable& tex)
{
    const auto& depthTex = tex.Get("t_depth");
    float depth = depthTex.Sample(input.UV).x; // берём красный канал (r = depth)
    // Инвертируем для наглядности: ближние пиксели ярче
    float brightness = (1.0f - depth) * 200.0f;
    return float4(brightness, brightness, brightness, 1.0f);
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
        float phi = PI * float(stack) / float(stacks);
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);

        for (int slice = 0; slice <= slices; ++slice)
        {
            float theta = 2.0f * PI * float(slice) / float(slices);
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

            vertices.push_back({ pos, norm, color, uv });
        }
    }

    for (int stack = 0; stack < stacks; ++stack)
    {
        for (int slice = 0; slice < slices; ++slice)
        {
            int first = stack * (slices + 1) + slice;
            int second = first + 1;
            int third = first + (slices + 1);
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

    DeviceContext& ctx = g_device->GetImmediateContext();

    // ── Подготовка матрицы MVP ─────────────────────────────
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

    // ── 1. Depth-only pass ─────────────────────────────────
    {
        PROFILE_SCOPE("Depth-Only Pass");

        // Привязываем только буфер глубины, рендер-таргет отключаем
        ctx.SetRenderTarget(nullptr, false);
        ctx.SetDepthBuffer(g_depthOnly.get());
        ctx.SetDepthWriteEnable(true);

        // Пиксельный шейдер не нужен (игнорируется растеризатором)
        ctx.SetPixelShader(nullptr);

        ctx.ClearDepth(1.0f);                // очистка глубины
        ctx.SetConstantBuffer(mvpCB);
        ctx.DrawIndexed();                   // рисуем сферу (только глубина!)
    }

    // ── 2. Визуализация глубины на весь экран ──────────────
    {
        PROFILE_SCOPE("Visualize Depth");

        // Рендерим в бэкбуфер, глубина не нужна
        ctx.SetRenderTarget(&g_device->GetBackBuffer(), false);
        ctx.SetDepthBuffer(nullptr);

        // Привязываем DepthTextureView как текстуру напрямую
        SamplerState sampler;
        sampler.filter = Filter::Bilinear;   // билинейная фильтрация глубины
        ctx.SetTexture("t_depth", g_depthView.get(), sampler);

        ctx.SetPixelShader(DepthVisualizePS);
        ctx.DrawFullScreenQuad();
    }

    g_device->Present();
}

void UpdateFPSCounter()
{
    static UINT64 frameCount = 0;
    static UINT64 lastUpdateTime = GetTickCount64();
    ++frameCount;

    UINT64 now = GetTickCount64();
    UINT64 elapsed = now - lastUpdateTime;

    if (elapsed >= 1000)
    {
        double fps = double(frameCount) * 1000.0 / double(elapsed);

        char title[128];
        sprintf_s(title, "SoftX — Depth-Only Pass (direct view) | FPS: %.1f", fps);
        SetWindowTextA(g_hWnd, title);

        frameCount = 0;
        lastUpdateTime = now;
    }
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
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
    wc.lpszClassName = L"SoftXDepthDemo";
    RegisterClassEx(&wc);

    // Create window
    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(
        0, L"SoftXDepthDemo", L"SoftX — Depth-Only Pass (direct view)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, nullptr);

    if (!g_hWnd)
        return -1;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Create device
    PresentParameters params;
    params.BackBufferSize = uint2(WINDOW_WIDTH, WINDOW_HEIGHT);
    params.hDeviceWindow = g_hWnd;
    params.Windowed = true;

    Device device(params);
    g_device = &device;

    // ── Ресурсы для depth‑only ────────────────────────────
    g_depthOnly = std::make_unique<DepthBuffer>(uint2(WINDOW_WIDTH, WINDOW_HEIGHT));
    g_depthView = std::make_unique<DepthTextureView>(g_depthOnly.get());

    // Configure context
    DeviceContext& ctx = g_device->GetImmediateContext();
    ctx.SetViewport(Viewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f));

    // Build sphere geometry
    VertexBuffer vb;
    IndexBuffer  ib;
    CreateSphere(vb, ib, 3.0f, 64, 32);

    ctx.SetVertexBuffer(vb);
    ctx.SetIndexBuffer(ib);
    ctx.SetVertexShader(TransformVS);
    ctx.SetCullMode(CullMode::Back);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetTileSize(64);
    ctx.SetFillMode(FillMode::Solid);

    // Начальная привязка (backbuffer)
    ctx.SetRenderTarget(&device.GetBackBuffer(), false);

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
            UpdateFPSCounter();
            DrawFrame();
        }
    }

    return int(msg.wParam);
}
