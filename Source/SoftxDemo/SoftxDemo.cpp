// Test program for SoftX with perspective and view matrices
// Renders a single triangle with transformation

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

//#include "OptickCapture.h"

// Window dimensions
const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

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
	std::vector<VertexInput> vertices;
	std::vector<uint32_t> indices;

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
			vertices.push_back({pos, norm, color, uv});
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

void CreateCube(VertexBuffer& vb, IndexBuffer& ib, float size = 1.0f)
{
	std::vector<VertexInput> vertices;
	std::vector<uint32_t> indices;

	float half = size * 0.5f;

	float3 corners[8] = {float3(-half, -half, -half), float3(half, -half, -half), float3(half, half, -half),
						 float3(-half, half, -half),  float3(-half, -half, half), float3(half, -half, half),
						 float3(half, half, half),	  float3(-half, half, half)};

	float3 normals[6] = {float3(0, 0, -1), float3(0, 0, 1),	 float3(0, -1, 0),
						 float3(0, 1, 0),  float3(-1, 0, 0), float3(1, 0, 0)};

	float4 colors[6] = {float4(1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, 0, 1, 1),
						float4(1, 1, 0, 1), float4(1, 0, 1, 1), float4(0, 1, 1, 1)};

	int faceIndices[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {0, 4, 5, 1}, {3, 2, 6, 7}, {0, 3, 7, 4}, {1, 5, 6, 2}};

	for (int f = 0; f < 6; ++f)
	{
		int* idx = faceIndices[f];
		float3 normal = normals[f];
		float4 color = colors[f];

		float2 uv[4] = {float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 1)};

		int start = (int)vertices.size();
		for (int v = 0; v < 4; ++v)
		{
			vertices.push_back({corners[idx[v]], normal, color, uv[v]});
		}

		indices.push_back(start);
		indices.push_back(start + 1);
		indices.push_back(start + 2);
		indices.push_back(start);
		indices.push_back(start + 2);
		indices.push_back(start + 3);
	}

	vb = VertexBuffer(std::move(vertices));
	ib = IndexBuffer(std::move(indices));
}

void DrawFrame()
{
	PROFILE_SCOPE("DrawFrame");

	DeviceContext& ctx = g_device->GetImmediateContext();

	{
		PROFILE_SCOPE("Setup matrices");
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
		float4x4 model = rotation_y(angle) * scaling(2.0f);

		// Combined MVP matrix
		float4x4 mvp = model * view * projection;

		// Create constant buffer with MVP
		ConstantBufferData cbData;
		cbData.modelViewProjection = mvp;

		ConstantBuffer cb(&cbData, sizeof(cbData));
		ctx.SetConstantBuffer(cb);
	}

	// Clear back buffer and depth buffer
	{
		PROFILE_SCOPE("Clearing backbuffer and depth");

		float4 color = float4(0.000f, 0.250f, 0.000f, 1.0f);
		ctx.Clear(color);
		ctx.ClearDepth(1.0f);
	}

	// Draw triangles
	{
		PROFILE_SCOPE("Draw");
		ctx.DrawIndexed();
	}

	// Present to screen
	{
		PROFILE_SCOPE("Present");
		g_device->Present();
	}
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

	// Set viewport
	Viewport vp(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f);
	ctx.SetViewport(vp);

	VertexBuffer vb;
	IndexBuffer ib;
	CreateSphere(vb, ib, 1.0f, 64, 32);

	// Set buffers
	ctx.SetVertexBuffer(vb);
	ctx.SetIndexBuffer(ib);

	// Set shaders
	ctx.SetVertexShader(TransformVS);
	ctx.SetPixelShader(ColorPS);

	ctx.SetFillMode(FillMode::Solid);
	ctx.SetCullMode(CullMode::Back);

	ctx.SetTileSize(64);

	//OptickCapture::Get().Initialize();

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
			PROFILE_FRAME("SoftX");
			//OptickCapture::Get().OnFrame();
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
		//if (wParam == 'P') // захватить 100 кадров
			//OptickCapture::Get().StartCapturing(100);
		//if (wParam == 'O') // переключатель
			//OptickCapture::Get().SwitchProfiler();
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
