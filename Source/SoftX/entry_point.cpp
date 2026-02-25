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
int2 Resolution = int2(1280, 1024);

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
	out.UV = float2(0, 0);
	return out;
}

float4 psColor(const VertexOutput& in, const void* constants)
{
	return in.Color;
}

void CreateCube(std::vector<VertexInput>& vertices, std::vector<uint32_t>& indices)
{
	vertices.clear();
	indices.clear();

	// Координаты вершин куба (от -1 до 1)
	float3 corners[8] = {
		float3(-1, -1, -1), // 0
		float3(1, -1, -1),	// 1
		float3(1, 1, -1),	// 2
		float3(-1, 1, -1),	// 3
		float3(-1, -1, 1),	// 4
		float3(1, -1, 1),	// 5
		float3(1, 1, 1),	// 6
		float3(-1, 1, 1)	// 7
	};

	// Цвета для каждой грани (R, G, B, A)
	float4 colors[6] = {
		float4(1, 0, 0, 1), // +X (красный)
		float4(0, 1, 0, 1), // -X (зелёный)
		float4(0, 0, 1, 1), // +Y (синий)
		float4(1, 1, 0, 1), // -Y (жёлтый)
		float4(0, 1, 1, 1), // +Z (голубой)
		float4(1, 0, 1, 1)	// -Z (фиолетовый)
	};

	// Определяем грани: для каждой грани задаём 4 индекса из corners[] и номер цвета
	int faces[6][4] = {
		{1, 5, 6, 2}, // +X (правая)
		{0, 3, 7, 4}, // -X (левая)
		{3, 2, 6, 7}, // +Y (верх)
		{0, 4, 5, 1}, // -Y (низ)
		{4, 7, 6, 5}, // +Z (передняя)
		{0, 1, 2, 3}  // -Z (задняя)
	};

	// Для каждой грани создаём 4 вершины (с одинаковым цветом) и 2 треугольника
	for (int f = 0; f < 6; ++f)
	{
		int idx[4] = {faces[f][0], faces[f][1], faces[f][2], faces[f][3]};
		float4 col = colors[f];

		// Добавляем 4 вершины
		int start = (int)vertices.size();
		vertices.push_back({corners[idx[0]], col});
		vertices.push_back({corners[idx[1]], col});
		vertices.push_back({corners[idx[2]], col});
		vertices.push_back({corners[idx[3]], col});

		// Первый треугольник (0,1,2)
		indices.push_back(start);
		indices.push_back(start + 1);
		indices.push_back(start + 2);

		// Второй треугольник (0,2,3)
		indices.push_back(start);
		indices.push_back(start + 2);
		indices.push_back(start + 3);
	}
}

void RenderFrame()
{
	static std::vector<TimerRecord> timers;
	timers.clear(); // очищаем для нового кадра

	ScopedTimer totalTimer("Total", timers);

	// Измеряем время с предыдущего кадра
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	g_deltaTime = float(double(currentTime.QuadPart - g_prevTime.QuadPart) / g_freq.QuadPart);
	g_prevTime = currentTime;

	// Ограничиваем дельту, чтобы избежать рывков при отладке (например, если стояли в брейкпоинте)
	if (g_deltaTime > 0.1f)
		g_deltaTime = 0.1f;

	// Обновляем углы с фиксированной угловой скоростью (радиан в секунду)
	static float angleX = 0.0f;
	static float angleY = 0.0f;
	angleX += 1.0f * g_deltaTime; // 1 радиан/сек
	angleY += 2.0f * g_deltaTime; // 2 радиана/сек

	float3 eye(0, 0, -50);
	float3 target(0, 0, 0);
	float3 up(0, 1, 0);

	float4x4 view = lookAtLH(eye, target, up);

	float4x4 model = rotationY(angleY) * rotationX(angleX);
	float aspect = (float)g_pDevice->GetPresentParams().BackBufferSize.x / g_pDevice->GetPresentParams().BackBufferSize.y;
	float4x4 proj = perspectiveLH(DegToRad(67.5f), aspect, 0.1f, 100.0f);

	TransformCB cb;
	cb.wvp = proj * view * model;

	{
		ScopedTimer t("Clear", timers);
		g_pDevice->Clear(float4(0.2f, 0.2f, 0.2f, 1.0f));
		g_pDevice->ClearDepth(1.0f);
	}

	{
		ScopedTimer t("Preparing pipeline", timers);
		g_pDevice->EnableTiledRendering(true);
		g_pDevice->SetTileSize(512);
		g_pDevice->SetVertexShader(vsTransform);
		g_pDevice->SetPixelShader(psColor);
		g_pDevice->SetConstantBuffer(&cb, sizeof(cb));
		g_pDevice->SetVertexBuffer(vertices);
		g_pDevice->SetIndexBuffer(indices);
	}

	{
		ScopedTimer t("DrawIndexed", timers);
		g_pDevice->DrawIndexed((uint32_t)indices.size(), 0);
	}

	{
		ScopedTimer t("Present", timers);
		g_pDevice->Present();
	}

	for (const auto& record : timers)
	{
		char buf[256];
		sprintf_s(buf, "%s: %.3f ms\n", record.name.c_str(), record.elapsedMs);
		OutputDebugStringA(buf);
	}
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

	HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"Software Renderer - Triangle Test",
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
		// Рендерим кадр
		RenderFrame();
		// Обновляем FPS (можно вызвать внутри RenderFrame или после)
		UpdateFPS(hWnd);
	}

	g_pDevice = nullptr;
	return 0;
}