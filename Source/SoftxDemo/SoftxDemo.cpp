#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>
#include <cmath>
#include <ctime>

#include <SoftX.h>

using namespace SoftX;
using namespace AfterMath;

static constexpr int WINDOW_WIDTH = 1280;
static constexpr int WINDOW_HEIGHT = 768;
static constexpr int CUBE_COUNT = 100;

static HWND g_hWnd = nullptr;
static Device* g_device = nullptr;
static const int QUERY_POOL_SIZE = 2;
static OcclusionQuery g_queryPool[QUERY_POOL_SIZE];
static int g_currentQueryIndex = 0;
static OcclusionQuery* g_pendingQuery = nullptr;

struct Mesh
{
    VertexBuffer           vb;
    IndexBuffer            ib;
    float4x4               worldMatrix;
    OcclusionQuery::queryID queryId = 0;
    bool                   visible = true;
};

struct CbData
{
    float4x4 modelViewProjection;
    float4 objectTypeColor;
};

struct CbDataQuery
{
    float4x4 modelViewProjection;
};

void CreateCube(VertexBuffer& vb, IndexBuffer& ib, const float4& color)
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
    const Face faces[6] = {
        // +X
        {{1, 5, 6, 2}, float3(1,0,0)},
        // -X
        {{4, 0, 3, 7}, float3(-1,0,0)},
        // +Y
        {{2, 6, 7, 3}, float3(0,1,0)},
        // -Y
        {{4, 5, 1, 0}, float3(0,-1,0)},
        // +Z
        {{5, 4, 7, 6}, float3(0,0,1)},
        // -Z
        {{0, 1, 2, 3}, float3(0,0,-1)}
    };

    std::vector<VertexInput> vertices;
    std::vector<uint>        indices;

    for (const auto& face : faces)
    {
        uint base = (uint)vertices.size();
        for (int idx : face.i)
            vertices.push_back({ positions[idx], face.normal, color });

        // Two triangles: 0-1-2 и 2-3-0 (CCW)
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);
    }

    vb = VertexBuffer(std::move(vertices));
    ib = IndexBuffer(std::move(indices));
}

VertexOutput MainVS(const VertexInput& input, const ConstantBuffer& cb, const TextureTable& tex)
{
    (void)tex;
    const CbData* data = reinterpret_cast<const CbData*>(cb.Data());
    VertexOutput output;
    output.Position = float4(input.Position.x, input.Position.y, input.Position.z, 1.0f) * data->modelViewProjection;
    output.Color = input.Color;
    output.Normal = input.Normal;
    output.UV = input.UV;
    return output;
}

float4 MainPS(const VertexOutput& input, const ConstantBuffer& cb, const TextureTable& tex)
{
    (void)tex;
    const CbData* data = reinterpret_cast<const CbData*>(cb.Data());

    return data->objectTypeColor;
}

VertexOutput OcclusionVS(const VertexInput& input, const ConstantBuffer& cb)
{
    const CbDataQuery* data = reinterpret_cast<const CbDataQuery*>(cb.Data());
    VertexOutput output;
    output.Position = float4(input.Position.x, input.Position.y, input.Position.z, 1.0f) * data->modelViewProjection;
    output.Color = float4(0, 0, 0, 0);
    output.Normal = float3(0, 0, 0);
    output.UV = float2(0, 0);
    return output;
}

float randomFloat(float min, float max)
{
    return min + (max - min) * (float(rand()) / RAND_MAX);
}

void DrawFrame(std::vector<Mesh>& cubes, Mesh& occluder, float occluderAngle)
{
    PROFILE_SCOPE("DrawFrame");

    DeviceContext& ctx = g_device->GetImmediateContext();

    const float aspect = float(WINDOW_WIDTH) / float(WINDOW_HEIGHT);
    float4x4 proj = perspective(Constants::degrees_to_radians(60.0f), aspect);
    float3 eye(0.0f, 30.0f, -1.0f);
    float3 target(0.0f, 0.0f, 0.0f);
    float4x4 view = look_at(eye, target);

    ctx.ClearColorAndDepth(float4(0.05f, 0.10f, 0.18f, 1.0f), 1.0f);

    {
        PROFILE_SCOPE("Draw occluder");
        float3 occluderPos(0.25f * cosf(occluderAngle), 2.0f, 1.0f * sinf(occluderAngle));
        float4x4 occluderWorld = translation(occluderPos) * scaling(float3(40, 0.5f, 15));
        float4x4 mvp = occluderWorld * view * proj;
        CbData cbData;
        cbData.modelViewProjection = mvp;
        cbData.objectTypeColor = float4(0.6f, 0.6f, 0.6f, 1.0f);
        ConstantBuffer cb(&cbData, sizeof(cbData));

        ctx.SetConstantBuffer(cb);
        ctx.SetVertexBuffer(occluder.vb);
        ctx.SetIndexBuffer(occluder.ib);
        ctx.SetVertexShader(MainVS);
        ctx.SetPixelShader(MainPS);
        ctx.SetTileSize(WINDOW_WIDTH / 16);
        ctx.DrawIndexed();
    }

    if (g_pendingQuery && g_pendingQuery->IsReady())
    {
        PROFILE_SCOPE("Retrieve last frame query results");
        for (auto& cube : cubes)
        {
            uint visibleSamples = 0;
            if (g_pendingQuery->GetResult(cube.queryId, &visibleSamples))
                cube.visible = (visibleSamples > 0);
            else
                cube.visible = false;
        }
        g_pendingQuery = nullptr;
    }

    {
        PROFILE_SCOPE("Begin new occlusion query");

        OcclusionQuery& currentQuery = g_queryPool[g_currentQueryIndex];

        if (!currentQuery.IsReady())
            currentQuery.Flush();

        g_currentQueryIndex = (g_currentQueryIndex + 1) % QUERY_POOL_SIZE;

        currentQuery.SetDepthBuffer(ctx.GetDepthBuffer());
        currentQuery.SetViewport(ctx.GetViewport());
        currentQuery.Begin();

        for (auto& cube : cubes)
        {
            float4x4 mvp = cube.worldMatrix * view * proj;
            CbDataQuery cbData;
            cbData.modelViewProjection = mvp;
            ConstantBuffer cb(&cbData, sizeof(cbData));

            currentQuery.SetVertexBuffer(cube.vb);
            currentQuery.SetIndexBuffer(cube.ib);
            currentQuery.SetConstantBuffer(cb);
            currentQuery.SetVertexShader(OcclusionVS);

            cube.queryId = currentQuery.DrawIndexed();
        }

        currentQuery.End();
        g_pendingQuery = &currentQuery;
    }

    int visibleCount = 0;
    {
        PROFILE_SCOPE("Draw visible cubes");
        for (auto& cube : cubes)
        {
            if (!cube.visible) continue;
            ++visibleCount;

            float4x4 mvp = cube.worldMatrix * view * proj;
            CbData cbData;
            cbData.modelViewProjection = mvp;
            cbData.objectTypeColor = float4(0.2f, 0.8f, 0.3f, 1.0f);
            ConstantBuffer cb(&cbData, sizeof(cbData));

            ctx.SetConstantBuffer(cb);
            ctx.SetVertexBuffer(cube.vb);
            ctx.SetIndexBuffer(cube.ib);
            ctx.SetVertexShader(MainVS);
            ctx.SetPixelShader(MainPS);
            ctx.SetTileSize(WINDOW_WIDTH / 32);
            ctx.DrawIndexed();
        }
    }

    {
        PROFILE_SCOPE("Draw invisible cubes");

        ComparisonFunc prevDepthFunc = ctx.GetDepthFunc();
        FillMode prevFillMode = ctx.GetFillMode();
        bool prevDepthWrite = ctx.GetDepthWriteEnable();
        ctx.SetDepthFunc(ComparisonFunc::Always);
        ctx.SetDepthWriteEnable(false);
        ctx.SetFillMode(FillMode::Wireframe);

        for (auto& cube : cubes)
        {
            if (cube.visible) continue;

            float4x4 mvp = cube.worldMatrix * view * proj;
            CbData cbData;
            cbData.modelViewProjection = mvp;
            cbData.objectTypeColor = float4(0.8f, 0.2f, 0.3f, 1.0f);
            ConstantBuffer cb(&cbData, sizeof(cbData));

            ctx.SetConstantBuffer(cb);
            ctx.SetVertexBuffer(cube.vb);
            ctx.SetIndexBuffer(cube.ib);
            ctx.SetVertexShader(MainVS);
            ctx.SetPixelShader(MainPS);
            ctx.SetTileSize(WINDOW_WIDTH / 32);
            ctx.DrawIndexed();
        }

        ctx.SetFillMode(prevFillMode);
        ctx.SetDepthFunc(prevDepthFunc);
        ctx.SetDepthWriteEnable(prevDepthWrite);
    }

    static char title[256];
    sprintf_s(title, "SoftX Occlusion Query Demo | Visible cubes: %d / %d", visibleCount, CUBE_COUNT);
    SetWindowTextA(g_hWnd, title);

    g_device->Present();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) DestroyWindow(hWnd);
        return 0;
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
    wc.lpszClassName = "SoftXOcclusionDemo";
    RegisterClassEx(&wc);

    RECT rc = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hWnd = CreateWindowEx(0, "SoftXOcclusionDemo", "SoftX Occlusion Query Demo",
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top,
                            nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) return -1;
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    PresentParameters params;
    params.BackBufferSize = uint2(WINDOW_WIDTH, WINDOW_HEIGHT);
    params.hDeviceWindow = g_hWnd;
    params.Windowed = true;

    std::unique_ptr<Device> device = std::make_unique<Device>(params);
    g_device = device.get();

    DeviceContext& ctx = g_device->GetImmediateContext();
    ctx.SetRenderTarget(device->GetBackBuffer(), true);
    ctx.SetViewport(Viewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f));
    ctx.SetTileSize(128);
    ctx.SetCullMode(CullMode::Back);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetFillMode(FillMode::Solid);
    ctx.SetTileSize(64);

    srand((unsigned)time(nullptr));

    std::vector<Mesh> cubes(CUBE_COUNT);
    for (int i = 0; i < CUBE_COUNT; ++i)
    {
        float3 pos(randomFloat(-30.0f, 30.0f), randomFloat(-2.5f, 0.0f), randomFloat(-20.0f, 20.0f));
        float4 color(0.2f, 0.8f, 0.3f, 1.0f);

        CreateCube(cubes[i].vb, cubes[i].ib, color);
        cubes[i].worldMatrix = translation(pos) * scaling(float3(1.0f));
    }

    Mesh occluder;
    CreateCube(occluder.vb, occluder.ib, float4(0.6f, 0.6f, 0.6f, 1.0f));

    MSG msg = {};
    float occluderAngle = 0.0f;

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
            DrawFrame(cubes, occluder, occluderAngle);
            occluderAngle += 0.015f;
        }
    }

    return int(msg.wParam);
}
