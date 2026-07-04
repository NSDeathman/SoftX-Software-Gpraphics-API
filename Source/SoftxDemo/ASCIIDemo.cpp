// ConsoleCubeDemo.cpp
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <conio.h>
#include <cmath>
#include <vector>
#include <SoftX.h>

#pragma comment(lib, "SoftX.lib")

using namespace SoftX;
using namespace AfterMath;

// ─── Настройки ─────────────────────────────────────────────
constexpr float CONSOLE_PIXEL_ASPECT = 0.5f;
constexpr int CONSOLE_WIDTH = 120;
constexpr int CONSOLE_HEIGHT = 60;
constexpr int BACKBUFFER_WIDTH = CONSOLE_WIDTH * 4;
constexpr int BACKBUFFER_HEIGHT = CONSOLE_HEIGHT * 4;
constexpr int TILE_SIZE = 32;

// ─── Константный буфер (расширен) ──────────────────────────
struct CbData
{
    float4x4 modelViewProjection;
    float4x4 world;
};

// ─── Шейдеры (с освещением) ────────────────────────────────
VertexOutput MainVS(const VertexInput& in, const ConstantBuffer& cb, const TextureTable&)
{
    const CbData& data = *reinterpret_cast<const CbData*>(cb.Data());
    VertexOutput out;

    out.Position = float4(in.Position, 1.0f) * data.modelViewProjection;

    // Нормаль в мировое пространство
    float4 worldNormal = float4(in.Normal, 0.0f) * data.world;
    worldNormal = normalize(worldNormal);
    out.Normal = worldNormal.xyz();

    out.Color = in.Color;
    out.UV = in.UV;
    return out;
}

float4 MainPS(const VertexOutput& in, const ConstantBuffer&, const TextureTable&)
{
    const float3 lightDir = normalize(float3(0.5f, 1.0f, -0.3f));
    float3 N = normalize(in.Normal);
    float NdotL = std::max(dot(N, lightDir), 0.0f);

    float3 ambient = in.Color.xyz() * 0.2f;
    float3 diffuse = in.Color.xyz() * NdotL;
    float3 lighting = ambient + diffuse;

    return float4(lighting, 1.0f);
}

// ─── Генерация сферы (радиус 1, центр в 0) ──────────────────
void CreateSphere(VertexBuffer& vb, IndexBuffer& ib,
    const float4& color,
    int slices = 32, int stacks = 16)
{
    std::vector<VertexInput> verts;
    std::vector<uint>        inds;

    // Параметризация:
    // phi   [0, PI]    – широта (от верхнего полюса)
    // theta [0, 2*PI]  – долгота
    // Ось Y вверх, камера смотрит вдоль +Z

    const float radius = 1.0f;
    for (int i = 0; i <= stacks; ++i)
    {
        float phi = (float)i / stacks * 3.14159265359f; // от 0 до PI
        float sinPhi = sinf(phi);
        float cosPhi = cosf(phi);

        for (int j = 0; j <= slices; ++j)
        {
            float theta = (float)j / slices * 2.0f * 3.14159265359f;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            float x = radius * sinPhi * cosTheta;
            float y = radius * cosPhi;          // Y вверх
            float z = radius * sinPhi * sinTheta;

            // Нормаль для единичной сферы равна позиции
            float3 pos(x, y, z);
            float3 normal = normalize(pos);

            verts.push_back({ pos, normal, color, float2(0,0) });
        }
    }

    // Индексы (против часовой для внешней стороны)
    for (int i = 0; i < stacks; ++i)
    {
        for (int j = 0; j < slices; ++j)
        {
            uint first = i * (slices + 1) + j;
            uint second = first + slices + 1;

            // Два треугольника на квад
            inds.push_back(first);
            inds.push_back(second);
            inds.push_back(first + 1);

            inds.push_back(second);
            inds.push_back(second + 1);
            inds.push_back(first + 1);
        }
    }

    vb = VertexBuffer(std::move(verts));
    ib = IndexBuffer(std::move(inds));
}

int main()
{
    VertexBuffer vb;
    IndexBuffer  ib;
    CreateSphere(vb, ib, float4(1.0f, 0.3f, 0.2f, 1.0f), 32, 16);

    PresentParameters params;
    params.BackBufferSize = uint2(BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT);
    params.Output = PresentationMode::Console;
    params.ConsoleSize = uint2(CONSOLE_WIDTH, CONSOLE_HEIGHT);

    Device device(params);
    DeviceContext& ctx = device.GetImmediateContext();

    ctx.SetRenderTarget(&device.GetBackBuffer(), true);
    ctx.SetViewport(Viewport(0, 0, BACKBUFFER_WIDTH, BACKBUFFER_HEIGHT, 0.0f, 1.0f));
    ctx.SetCullMode(CullMode::Back);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetFillMode(FillMode::Solid);
    ctx.SetTileSize(TILE_SIZE);

    float aspect = float(BACKBUFFER_WIDTH) / float(BACKBUFFER_HEIGHT) * CONSOLE_PIXEL_ASPECT;
    float4x4 proj = perspective(radians(60.0f), aspect);
    float4x4 view = look_at(float3(0, 0, -2), float3(0, 0, 0));

    float angle = 0.0f;

    while (true)
    {
        ctx.ClearColorAndDepth(float4(0.0f, 0.0f, 0.0f, 1.0f), 1.0f);

        angle += 0.03f;
        float4x4 world = rotation_y(angle) * rotation_x(angle * 0.5f);
        float4x4 mvp = world * view * proj;

        CbData cb;
        cb.modelViewProjection = mvp;
        cb.world = world;
        ConstantBuffer cbuffer(&cb, sizeof(cb));

        ctx.SetConstantBuffer(cbuffer);
        ctx.SetVertexBuffer(vb);
        ctx.SetIndexBuffer(ib);
        ctx.SetVertexShader(MainVS);
        ctx.SetPixelShader(MainPS);

        ctx.DrawIndexed();
        device.Present();
    }

    return 0;
}
