#include <windows.h>
#include <cmath>
#include "Device.h"
#include "ScopedTimer.h"

bool g_running = true;
int g_frameCount = 0;
LARGE_INTEGER g_freq;
LARGE_INTEGER g_lastFPSTime;
LARGE_INTEGER g_prevTime;
float g_deltaTime = 0.016f;
float g_fps = 0.0f;
wchar_t g_windowTitle[256] = L"SoftX - FPS: ?";
int2 Resolution = int2(1280, 720);

void UpdateFPS(HWND hWnd)
{
	g_frameCount++;

	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);

	double elapsed = double(currentTime.QuadPart - g_lastFPSTime.QuadPart) / g_freq.QuadPart;
	if (elapsed >= 1.0)
	{
		g_fps = g_frameCount / (float)elapsed;

		// Формируем заголовок
		swprintf(g_windowTitle, 256, L"SoftX - FPS: %.1f", g_fps);
		SetWindowTextW(hWnd, g_windowTitle);

		// Сброс
		g_frameCount = 0;
		g_lastFPSTime = currentTime;
	}
}

std::vector<VertexInput> vertices = {};
std::vector<uint32_t> indices = {};

Device* g_pDevice = nullptr;

struct TransformCB
{
	float4x4 wvp; // world-view-projection matrix
};

VertexOutput vsTransform(const VertexInput& in, const void* constants)
{
	const TransformCB* cb = static_cast<const TransformCB*>(constants);
	VertexOutput out;
	out.Position = cb->wvp * float4(in.Position.x, in.Position.y, in.Position.z, 1.0f);
	out.Color = in.Color;
	out.UV = in.UV; // теперь передаём UV дальше
	return out;
}

void CreateCube(std::vector<VertexInput>& vertices, std::vector<uint32_t>& indices)
{
	vertices.clear();
	indices.clear();

	float3 corners[8] = {float3(-1, -1, -1), float3(1, -1, -1), float3(1, 1, -1), float3(-1, 1, -1),
						 float3(-1, -1, 1),	 float3(1, -1, 1),	float3(1, 1, 1),  float3(-1, 1, 1)};

	float4 colors[6] = {float4(1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, 0, 1, 1),
						float4(1, 1, 0, 1), float4(0, 1, 1, 1), float4(1, 0, 1, 1)};

	int faces[6][4] = {{1, 5, 6, 2}, {0, 3, 7, 4}, {3, 2, 6, 7}, {0, 4, 5, 1}, {4, 7, 6, 5}, {0, 1, 2, 3}};

	for (int f = 0; f < 6; ++f)
	{
		int idx[4] = {faces[f][0], faces[f][1], faces[f][2], faces[f][3]};
		float4 col = colors[f];

		int start = (int)vertices.size();
		vertices.push_back({corners[idx[0]], col, float2(0, 0)});
		vertices.push_back({corners[idx[1]], col, float2(1, 0)});
		vertices.push_back({corners[idx[2]], col, float2(1, 1)});
		vertices.push_back({corners[idx[3]], col, float2(0, 1)});

		indices.push_back(start);
		indices.push_back(start + 1);
		indices.push_back(start + 2);
		indices.push_back(start);
		indices.push_back(start + 2);
		indices.push_back(start + 3);
	}
}

void RenderFrame()
{
	// Текстура для промежуточного рендера (создаётся один раз)
	static RenderTargetTexture rt(int2(800, 600));

	// --- Проход 1: рендерим градиент в текстуру rt ---
	g_pDevice->SetRenderTarget(&rt);
	g_pDevice->Clear(float4(0, 0, 0, 1));
	g_pDevice->ClearDepth(1.0f);

	// Пиксельный шейдер для генерации цветного узора
	auto psGenerate = [](const VertexOutput& in, const void*) -> float4 {
		float2 uv = in.UV;
		float r = 0.5f + 0.5f * sinf(uv.x * 20.0f);
		float g = 0.5f + 0.5f * sinf(uv.y * 20.0f);
		float b = 0.5f + 0.5f * sinf((uv.x + uv.y) * 20.0f);
		return float4(r, g, b, 1.0f);
	};
	g_pDevice->SetPixelShader(psGenerate);
	g_pDevice->SetVertexShader(nullptr);
	g_pDevice->DrawFullScreenQuad();

	// --- Проход 2: рисуем куб, текстурированный содержимым rt ---
	g_pDevice->SetRenderTarget(nullptr); // backbuffer
	g_pDevice->Clear(float4(0.2f, 0.2f, 0.2f, 1.0f));
	g_pDevice->ClearDepth(1.0f);

	// Матрицы
	static float angle = 0.0f;
	angle += 0.01f;
	float3 eye(0, 0, -50);
	float3 target(0, 0, 0);
	float3 up(0, 1, 0);
	float4x4 view = lookAtLH(eye, target, up);
	float4x4 model = rotationY(angle) * rotationX(angle * 0.75f) * scaling(2);
	float aspect = (float)g_pDevice->GetBackBuffer().width() / g_pDevice->GetBackBuffer().height();
	float4x4 proj = perspectiveLH(DegToRad(67), aspect, 0.1f, 100.0f);
	TransformCB cb;
	cb.wvp = proj * view * model;

	// Пиксельный шейдер, захватывающий текстуру rt по ссылке
	auto psTextured = [&](const VertexOutput& in, const void*) -> float4 { return rt.texture().sample(in.UV); };
	g_pDevice->SetVertexShader(vsTransform);
	g_pDevice->SetPixelShader(psTextured);
	g_pDevice->SetConstantBuffer(&cb, sizeof(cb));
	g_pDevice->SetVertexBuffer(vertices);
	g_pDevice->SetIndexBuffer(indices);
	g_pDevice->DrawIndexed((uint32_t)indices.size(), 0);

	g_pDevice->Present();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		g_running = false; // сигнал для выхода из цикла
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			DestroyWindow(hWnd);
		}
		if (wParam == '1') g_pDevice->SetFillMode(FillMode::Point);
		if (wParam == '2') g_pDevice->SetFillMode(FillMode::Wireframe);
		if (wParam == '3') g_pDevice->SetFillMode(FillMode::Solid);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	const wchar_t CLASS_NAME[] = L"TriangleTest";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"SoftX Software Renderer",
							   WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
							   Resolution.x, Resolution.y, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return -1;

	QueryPerformanceFrequency(&g_freq);
	QueryPerformanceFrequency(&g_lastFPSTime);
	QueryPerformanceCounter(&g_lastFPSTime);
	QueryPerformanceCounter(&g_prevTime);

	PresentParameters pp;
	pp.BackBufferSize = Resolution;
	pp.hDeviceWindow = hWnd;
	pp.Windowed = true;

	Device device(pp);
	g_pDevice = &device;

	ShowWindow(hWnd, nCmdShow);

	CreateCube(vertices, indices);

	// --- Активный цикл рендеринга ---
	MSG msg = {};
	while (g_running)
	{
		// Обрабатываем все накопившиеся сообщения
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		PREPARE_TIMERS();
		{
			PROFILE_SCOPE("RenderFrame");
			RenderFrame();
		}
		UpdateFPS(hWnd);
		LOG_PROFILE();
	}

	g_pDevice = nullptr;
	return 0;
}