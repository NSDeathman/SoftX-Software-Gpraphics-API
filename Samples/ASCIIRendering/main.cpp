/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include <chrono>
#include <conio.h>
#include <SoftX.h>
#include <AfterMath.h>
/////////////////////////////////////////////////////////////////
using namespace SoftX;
using namespace AfterMath;
/////////////////////////////////////////////////////////////////
Device* g_device = nullptr;
/////////////////////////////////////////////////////////////////
struct CbData
{
    float4x4 worldViewProjection;
    float4x4 world;
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
            Vertex v;
            v.Position = positions[idx];
            v.Attributes[0] = color;
            v.Attributes[1] = face.normal;
            vb.Add(v);
        }

        // Two triangles: 0-1-2 and 2-3-0 (CCW)
        ib.AddTri(base + 0, base + 1, base + 2);
        ib.AddTri(base + 2, base + 3, base + 0);
    }
}

int main()
{
    PresentParameters params;
    params.Output = PresentationMode::Console;
    params.ConsoleSize = uint2(100, 40);
    params.BackBufferSize = uint2(320, 240);

    Device device(params);
    g_device = &device;
    DeviceContext& ctx = g_device->GetImmediateContext();

    VertexBuffer vb;
    IndexBuffer ib;
    CreateCube(vb, ib, float4(0.0f, 1.0f, 0.0f, 1.0f));

    auto vs = [](const Vertex& Input, ConstantBuffer cb, const TextureTable&) -> Interpolant
    {
        const CbData* CBuf = reinterpret_cast<const CbData*>(cb.Data());
        Interpolant Output;
        Output.ClipSpacePosition = float4(Input.Position.xyz(), 1.0f) * CBuf->worldViewProjection;
        Output.Attributes[0] = Input.Position.xyz() * float3x3(CBuf->world);
        Output.Attributes[1] = normalize(Input.Attributes[1].xyz() * float3x3(CBuf->world));
        Output.Attributes[2] = Input.Attributes[0];
        return Output;
    };
    auto ps = [](const Interpolant& Input, ConstantBuffer, const TextureTable&) -> float4
    {
        float3 LightPos = float3(0.0f, 1.0f, -1.0f);
        float3 LightDir = normalize(LightPos - Input.Attributes[0].xyz());
        float DirectLighting = std::max(dot(Input.Attributes[1], LightDir), 0.0f);
        float IndirectLighting = 0.1f;
        float4 FinalColor = float4(DirectLighting + IndirectLighting) * Input.Attributes[2];
        return pow(FinalColor, 1.0f / 2.2f);
    };

    ctx.SetRenderTarget(device.GetBackBuffer(), false);
    ctx.SetViewport(Viewport(0.0f, 0.0f, params.BackBufferSize.x, params.BackBufferSize.y, 0.0f, 1.0f));
    ctx.SetCullMode(CullMode::None);
    ctx.SetDepthFunc(ComparisonFunc::Less);
    ctx.SetFillMode(FillMode::Solid);
    ctx.SetVertexBuffer(vb);
    ctx.SetIndexBuffer(ib);
    ctx.SetVertexShader(vs);
    ctx.SetPixelShader(ps);
    ctx.SetTileSize(128);

    auto lastTime = std::chrono::steady_clock::now();
    while (true)
    {
        if (_kbhit() && _getch() == 27) // 27 = Esc
            break;

        auto currentTime = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        const float aspect = float(params.BackBufferSize.x) / float(params.BackBufferSize.y);
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
        cData.worldViewProjection = mvp;
        cData.world = cubeWorld;
        ConstantBuffer cbuf(&cData, sizeof(cData));

        ctx.Clear(ClearFlags::All, float4(0.0f, 0.0f, 0.0f, 1.0f), 1.0f);
        ctx.SetConstantBuffer(cbuf);
        ctx.DrawIndexed();
        g_device->Present();
    }

	return 0;
}
/////////////////////////////////////////////////////////////////
