// Test program for SoftX with perspective and view matrices
// Renders a geometry with transformation
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>

// Include SoftX
#include <SoftX/SoftX.h>

#pragma comment(lib, "SoftX.lib")

using namespace SoftX;
using namespace AfterMath;

// Window dimensions
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

#define SIMPLE_TRIANGLE 0
#define SPHERE 1
#define CUBE 2

#define GEOMETRY_TO_RENDER SPHERE

// Global window handle
HWND g_hWnd = nullptr;

Device* g_device = nullptr;

// Forward declaration of window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Constant buffer structure
struct ConstantBufferData
{
	float4x4 modelViewProjection; // 64 bytes
};

// Vertex shader: transforms position by MVP matrix
VertexOutput TransformVS(const VertexInput& input, ConstantBuffer cb)
{
	// Cast constant buffer data
	const ConstantBufferData* data = reinterpret_cast<const ConstantBufferData*>(cb.Data());

	VertexOutput output;
	// Transform position: clipPos = MVP * float4(objectPos, 1.0f)
	float4 objectPos(input.Position.x, input.Position.y, input.Position.z, 1.0f);
	output.Position = objectPos * data->modelViewProjection;

	// Pass through attributes (they will be interpolated)
	output.Color = input.Color;
	output.Normal = input.Normal;
	output.UV = input.UV;

	return output;
}

// Simple pixel shader: interpolated vertex color
float4 ColorPS(const VertexOutput& input, ConstantBuffer cb)
{
	return input.Color; // use interpolated color
}

void CreateSphere(VertexBuffer& vb, IndexBuffer& ib, float radius, int slices, int stacks)
{
	vb.Clear();
	ib.Clear();
	for (int stack = 0; stack <= stacks; ++stack)
	{
		float phi = PI * stack / stacks;
		float sinPhi = sinf(phi), cosPhi = cosf(phi);
		for (int slice = 0; slice <= slices; ++slice)
		{
			float theta = 2.0f * PI * slice / slices;
			float sinTheta = sinf(theta), cosTheta = cosf(theta);
			float3 pos(radius * sinPhi * cosTheta, radius * cosPhi, radius * sinPhi * sinTheta);
			float3 norm = normalize(pos);
			float4 color((norm.x + 1.0f) * 0.5f, (norm.y + 1.0f) * 0.5f, (norm.z + 1.0f) * 0.5f, 1.0f);
			float2 uv((float)slice / slices, (float)stack / stacks);
			vb.Add({pos, norm, color, uv});
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
			ib.Add(first);
			ib.Add(second);
			ib.Add(third);
			ib.Add(second);
			ib.Add(fourth);
			ib.Add(third);
		}
	}
}

void CreateCube(VertexBuffer& vb, IndexBuffer& ib, float size = 1.0f)
{
    vb.Clear();
    ib.Clear();

    float half = size * 0.5f;

    // Координаты 8 углов куба
    float3 corners[8] = {
        float3(-half, -half, -half), // 0
        float3( half, -half, -half), // 1
        float3( half,  half, -half), // 2
        float3(-half,  half, -half), // 3
        float3(-half, -half,  half), // 4
        float3( half, -half,  half), // 5
        float3( half,  half,  half), // 6
        float3(-half,  half,  half)  // 7
    };

    // Нормали для шести граней
    float3 normals[6] = {
        float3( 0,  0, -1), // -Z (задняя)
        float3( 0,  0,  1), // +Z (передняя)
        float3( 0, -1,  0), // -Y (низ)
        float3( 0,  1,  0), // +Y (верх)
        float3(-1,  0,  0), // -X (левая)
        float3( 1,  0,  0)  // +X (правая)
    };

    // Цвета для каждой грани (для визуальной идентификации)
    float4 colors[6] = {
        float4(1, 0, 0, 1), // красный
        float4(0, 1, 0, 1), // зелёный
        float4(0, 0, 1, 1), // синий
        float4(1, 1, 0, 1), // жёлтый
        float4(1, 0, 1, 1), // пурпурный
        float4(0, 1, 1, 1)  // голубой
    };

    // Индексы углов для каждой грани (порядок обхода против часовой стрелки, если смотреть на грань)
    // Для левосторонней системы координат (Z вперёд) зададим грани так:
    int faceIndices[6][4] = {
        {0, 1, 2, 3}, // -Z (задняя)
        {5, 4, 7, 6}, // +Z (передняя)
        {0, 4, 5, 1}, // -Y (низ)
        {3, 2, 6, 7}, // +Y (верх)
        {0, 3, 7, 4}, // -X (левая)
        {1, 5, 6, 2}  // +X (правая)
    };

    for (int f = 0; f < 6; ++f) {
        int* idx = faceIndices[f];
        float3 normal = normals[f];
        float4 color = colors[f];

        // UV-координаты для четырёх вершин грани
        float2 uv[4] = {
            float2(0, 0),
            float2(1, 0),
            float2(1, 1),
            float2(0, 1)
        };

        int start = (int)vb.Size();
        for (int v = 0; v < 4; ++v) {
            vb.Add({ corners[idx[v]], normal, color, uv[v] });
        }

        // Два треугольника на грань: (0,1,2) и (0,2,3)
        ib.Add(start);
        ib.Add(start + 1);
        ib.Add(start + 2);
        ib.Add(start);
        ib.Add(start + 2);
        ib.Add(start + 3);
    }
}

void DrawFrame()
{
	DeviceContext& ctx = g_device->GetImmediateContext();

	// Setup matrices (column‑vector convention)
	float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;

	// Projection matrix: perspective, left‑handed, zero‑to‑one depth
	float4x4 projection = perspective(Constants::degrees_to_radians(30.0f), aspect);

	// View matrix: camera at (0,0,0) looking down +Z
	float3 eye(0.0f, 0.0f, -10.0f);
	float3 target(0.0f, 0.0f, 1.0f);
	float4x4 view = look_at(eye, target);

	static float angle = 0.0f;
	angle += 0.01f;

	// Model matrix: identity (object already in world space)
	float3x3 model = float3x3::scaling(2.0f) * float3x3::rotation_y(angle);

	// Combined MVP matrix
	float4x4 mvp = model * view * projection;

	// Create constant buffer with MVP
	ConstantBufferData cbData;
	cbData.modelViewProjection = mvp;

	ConstantBuffer cb(&cbData, sizeof(cbData));
	ctx.SetConstantBuffer(cb);

	// Clear back buffer and depth buffer
	float4 color = float4(0.1f, 0.5f, 0.1f, 1.0f);
	ctx.Clear(color);
	ctx.ClearDepth(1.0f);

	// Draw triangle
	ctx.DrawIndexed();

	// Present to screen
	g_device->Present();
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Register window class
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = L"SoftXTestWindow";
	RegisterClassEx(&wc);

	// Create window
	RECT rc = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd =
		CreateWindowEx(0, L"SoftXTestWindow", L"SoftX Triangle Test with Matrices", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
					   CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
		return -1;

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	// Create SoftX device
	PresentParameters params;
	params.BackBufferSize = int2(WINDOW_WIDTH, WINDOW_HEIGHT);
	params.hDeviceWindow = g_hWnd;
	params.Windowed = true;

	Device device(params);
	g_device = &device;

	DeviceContext& ctx = g_device->GetDeviceContext();

	// Set render target (back buffer) and depth buffer
	ctx.SetRenderTarget(&device.GetBackBuffer(), true);

	// Set shaders
	ctx.SetVertexShader(TransformVS);
	ctx.SetPixelShader(ColorPS);

	ctx.SetFillMode(FillMode::Solid);
	ctx.SetCullMode(CullMode::Back);

	ctx.SetTileRenderingState(true);
	ctx.SetTileSize(16);

	// Set viewport
	Viewport vp(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f);
	ctx.SetViewport(vp);

	VertexBuffer vb;
	IndexBuffer ib;

#if GEOMETRY_TO_RENDER == SIMPLE_TRIANGLE
	// Define triangle vertices in object space
	// A triangle with different colors at each vertex
	vb.Add(VertexInput(float3(-1.0f, -1.0f, 0.0f), float3(0, 0, 0), float4(1, 0, 0, 1)));			// red
	vb.Add(VertexInput(float3(1.0f, -1.0f, 0.0f), float3(0, 0, 0), float4(0, 1, 0, 1)));			// green
	vb.Add(VertexInput(float3(0.0f, 1.0f, 0.0f), float3(0, 0, 0), float4(0, 0, 1, 1)));				// blue

	// Index buffer
	ib.Add(0);
	ib.Add(1);
	ib.Add(2);

	ctx.SetCullMode(CullMode::None);

#elif GEOMETRY_TO_RENDER == SPHERE
	CreateSphere(vb, ib, 1.0f, 64, 32);
#elif GEOMETRY_TO_RENDER == CUBE
	CreateCube(vb, ib);
#endif

	// Set buffers
	ctx.SetVertexBuffer(vb);
	ctx.SetIndexBuffer(ib);

	// Main message loop
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
			DrawFrame();
		}
	}

	return (int)msg.wParam;
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
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
