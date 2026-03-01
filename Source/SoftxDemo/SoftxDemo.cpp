#define SOFTX_STATIC
#include <Windows.h>
#include <SoftX/SoftX.h>
#include <thread>
#include <vector>

#pragma comment(lib, "SoftX.lib")

using namespace SoftX;

bool g_running = true;

struct TransformCB
{
	float4x4 wvp;
};

struct LightCB
{
	float4x4 wvp;
	float4 lightDir;   // направление света (w не используется)
	float4 lightColor; // цвет света
	float4 ambient;	   // фоновое освещение
};

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		g_running = false;
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			DestroyWindow(hWnd);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

VertexOutput vsTransform(const VertexInput& in, ConstantBuffer cb)
{
	const TransformCB* transform = (const TransformCB*)cb.Data();
	VertexOutput out;
	out.Position = transform->wvp * float4(in.Position.x, in.Position.y, in.Position.z, 1.0f);
	out.Normal = in.Normal;
	out.Color = in.Color;
	out.UV = in.UV;
	return out;
}

void GSSplitTriangle(const VertexOutput in[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices)
{
	// Вычисляем середины рёбер
	VertexOutput mid0, mid1, mid2;

#define LERP(vertex, field, index0, index1) vertex.field = (in[index0].field + in[index1].field) * 0.5f

	LERP(mid0, Position, 0, 1);
	LERP(mid1, Position, 1, 2);
	LERP(mid2, Position, 2, 0);	
	
	LERP(mid0, Color, 0, 1);
	LERP(mid1, Color, 1, 2);
	LERP(mid2, Color, 2, 0);	
	
	LERP(mid0, Normal, 0, 1);
	LERP(mid1, Normal, 1, 2);
	LERP(mid2, Normal, 2, 0);	
	
	LERP(mid0, UV, 0, 1);
	LERP(mid1, UV, 1, 2);
	LERP(mid2, UV, 2, 0);

#undef LERP

	// Добавляем вершины (6 новых вершин)
	outVerts.push_back(in[0]);
	outVerts.push_back(mid0);
	outVerts.push_back(in[1]);
	outVerts.push_back(mid1);
	outVerts.push_back(in[2]);
	outVerts.push_back(mid2);

	// Индексы для 4 маленьких треугольников
	// Треугольник 1: in0 - mid0 - mid2
	outIndices.push_back(0);
	outIndices.push_back(1);
	outIndices.push_back(5);
	// Треугольник 2: mid0 - in1 - mid1
	outIndices.push_back(1);
	outIndices.push_back(2);
	outIndices.push_back(3);
	// Треугольник 3: mid2 - mid1 - in2
	outIndices.push_back(5);
	outIndices.push_back(3);
	outIndices.push_back(4);
	// Треугольник 4: mid0 - mid1 - mid2 (центральный)
	outIndices.push_back(1);
	outIndices.push_back(3);
	outIndices.push_back(5);
}

float4 psColor(const VertexOutput& in, ConstantBuffer /*cb*/)
{
	return in.Color;
}

float4 psLight(const VertexOutput& in, ConstantBuffer cb)
{
	const LightCB* light = (const LightCB*)cb.Data();

	// Нормализуем интерполированную нормаль (она может стать не единичной после интерполяции)
	float3 N = normalize(in.Normal);

	// Направление света (предполагаем, что оно уже нормализовано и направлено на источник)
	float3 L = normalize(light->lightDir.xyz());

	// Диффузное освещение (max(0, dot(N, L)))
	float diff = std::max(0.0f, dot(N, L));

	// Итоговый цвет = (ambient + diffuse * lightColor) * baseColor
	float3 finalColor = (light->ambient.xyz() + diff * light->lightColor.xyz()) * in.Color.xyz();

	return float4(finalColor, in.Color.w);
}

// Шейдер для отображения текстуры на весь экран
float4 psTexture(const VertexOutput& in, ConstantBuffer /*cb*/)
{
	// Здесь мы ожидаем, что текстура будет передана через глобальную переменную или через захват лямбды
	// Для простоты используем глобальную текстуру (но в демо мы передадим её через захват лямбды)
	// В реальном коде можно передавать через uniform.
	return float4(1, 0, 1, 1); // заглушка, будет заменено
}

void CreateSphere(VertexBuffer& vb, IndexBuffer& ib, float radius, int sliceCount, int stackCount)
{
	vb.Clear();
	ib.Clear();

	for (int stack = 0; stack <= stackCount; ++stack)
	{
		float phi = PI * stack / stackCount; // от 0 до PI
		float sinPhi = sinf(phi);
		float cosPhi = cosf(phi);

		for (int slice = 0; slice <= sliceCount; ++slice)
		{
			float theta = 2.0f * PI * slice / sliceCount; // от 0 до 2PI
			float sinTheta = sinf(theta);
			float cosTheta = cosf(theta);

			// Позиция на сфере
			float3 pos(radius * sinPhi * cosTheta, radius * cosPhi, radius * sinPhi * sinTheta);

			// Нормаль (для сферы с центром в 0 совпадает с нормализованной позицией)
			float3 normal = normalize(pos);

			// Цвет на основе нормали (опционально)
			float4 color((normal.x + 1.0f) * 0.5f, (normal.y + 1.0f) * 0.5f, (normal.z + 1.0f) * 0.5f, 1.0f);

			// Текстурные координаты
			float2 uv((float)slice / sliceCount, (float)stack / stackCount);

			vb.Add({pos, normal, color, uv});
		}
	}

	// Индексы для двух треугольников на каждый четырёхугольник
	for (int stack = 0; stack < stackCount; ++stack)
	{
		for (int slice = 0; slice < sliceCount; ++slice)
		{
			int first = stack * (sliceCount + 1) + slice;
			int second = first + 1;
			int third = first + (sliceCount + 1);
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

int main()
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
	const wchar_t CLASS_NAME[] = L"MultiThreadTest";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"SoftX Multi-Threaded Demo",
							   WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
							   800, 600, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return -1;
	ShowWindow(hWnd, SW_SHOW);

	PresentParameters pp;
	pp.BackBufferSize = int2(800, 600);
	pp.hDeviceWindow = hWnd;
	pp.Windowed = true;

	Device device(pp);
	auto& immediateCtx = device.GetImmediateContext();

	// Создаём общие ресурсы (вершинный и индексный буфер)
	VertexBuffer vb;
	IndexBuffer ib;
	CreateSphere(vb, ib, 1.0f, 32, 16);

	// Создаём две текстуры-рендертаргета (по 400x600 каждая)
	RenderTargetTexture rtLeft(int2(400, 600));
	RenderTargetTexture rtRight(int2(400, 600));

	// Создаём отложенные контексты
	auto ctxLeft = device.CreateDeferredContext();
	auto ctxRight = device.CreateDeferredContext();

	// Настраиваем левый контекст
	ctxLeft->SetRenderTarget(&rtLeft, true);
	ctxLeft->SetViewport(Viewport(0, 0, 400, 600));
	ctxLeft->SetVertexShader(vsTransform);
	ctxLeft->SetPixelShader(psLight);
	ctxLeft->SetVertexBuffer(vb);
	ctxLeft->SetIndexBuffer(ib);
	ctxLeft->SetCullMode(CullMode::Back);
	ctxLeft->SetFillMode(FillMode::Solid);
	ctxLeft->SetTileRenderingState(false);
	ctxLeft->SetTileSize(64);

	// Настраиваем правый контекст
	ctxRight->SetRenderTarget(&rtRight, true);
	ctxRight->SetViewport(Viewport(0, 0, 400, 600));
	ctxRight->SetVertexShader(vsTransform);
	ctxRight->SetGeometryShader(GSSplitTriangle);
	ctxRight->SetPixelShader(psLight);
	ctxRight->SetVertexBuffer(vb);
	ctxRight->SetIndexBuffer(ib);
	ctxRight->SetCullMode(CullMode::Back);
	ctxRight->SetFillMode(FillMode::Solid);
	ctxRight->SetTileRenderingState(false);
	ctxRight->SetTileSize(64);

	// Переменные для анимации
	float angle = 0.0f;
	LARGE_INTEGER freq, lastTime;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&lastTime);

	// Главный цикл
	MSG msg = {};
	while (g_running)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);
		float deltaTime = float(double(currentTime.QuadPart - lastTime.QuadPart) / freq.QuadPart);
		lastTime = currentTime;
		angle += deltaTime * 0.5f;

		float3 eye(0, 0, -50);
		float3 target(0, 0, 0);
		float3 up(0, 1, 0);
		float4x4 view = lookAtLH(eye, target, up);
		float4x4 modelLeft = rotationY(0);
		float aspect = 400.0f / 600.0f;
		float4x4 proj = perspectiveLH(3.14159f / 4.0f, aspect, 0.1f, 100.0f);
		LightCB cbLeft;
		cbLeft.wvp = proj * view * modelLeft;
		cbLeft.lightDir = float4(0, 1, 1, 0);
		cbLeft.lightColor = float4(1, 1, 1, 1);
		cbLeft.ambient = float4(0.2f, 0.2f, 0.2f, 1);
		ConstantBuffer cbLeftBuf(&cbLeft, sizeof(cbLeft));

		float4x4 modelRight = rotationY(0);
		LightCB cbRight;
		cbRight.wvp = proj * view * modelRight;
		cbRight.lightDir = float4(0, 1, 1, 0);
		cbRight.lightColor = float4(1, 1, 1, 1);
		cbRight.ambient = float4(0.2f, 0.2f, 0.2f, 1);
		ConstantBuffer cbRightBuf(&cbRight, sizeof(cbRight));

		// Запускаем потоки для отложенных контекстов
		std::thread t1([&]() {
			ctxLeft->SetConstantBuffer(cbLeftBuf);
			ctxLeft->Clear(float4(0.1f, 0.1f, 0.1f, 1.0f));
			ctxLeft->ClearDepth(1.0f);
			ctxLeft->DrawIndexed();
		});

		std::thread t2([&]() {
			ctxRight->SetConstantBuffer(cbRightBuf);
			ctxRight->Clear(float4(0.2f, 0.2f, 0.2f, 1.0f));
			ctxRight->ClearDepth(1.0f);
			ctxRight->DrawIndexed();
		});

		t1.join();
		t2.join();

		// Теперь в главном контексте выводим результаты на экран
		immediateCtx.SetRenderTarget(&device.GetBackBuffer(), false);
		immediateCtx.Clear(float4(0, 0, 0, 1));
		immediateCtx.ClearDepth(1.0f);

		// Пиксельный шейдер, который отображает две текстуры рядом
		auto psCombine = [&](const VertexOutput& in, ConstantBuffer) -> float4 {
			float2 uv = in.UV;
			if (uv.x < 0.5f)
			{
				// левая половина – текстура rtLeft, но UV нужно перемасштабировать
				float2 uvLeft(uv.x * 2.0f, uv.y);
				return rtLeft.texture().sample(uvLeft);
			}
			else
			{
				float2 uvRight((uv.x - 0.5f) * 2.0f, uv.y);
				return rtRight.texture().sample(uvRight);
			}
		};

		immediateCtx.SetPixelShader(psCombine);
		immediateCtx.DrawFullScreenQuad();

		device.Present();
	}

	return 0;
}
