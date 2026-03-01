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
	out.Color = in.Color;
	out.UV = in.UV;
	return out;
}

void GSSplitTriangle(const VertexOutput in[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices)
{
	// Вычисляем середины рёбер
	VertexOutput mid0, mid1, mid2;
	mid0.Position = (in[0].Position + in[1].Position) * 0.5f;
	mid1.Position = (in[1].Position + in[2].Position) * 0.5f;
	mid2.Position = (in[2].Position + in[0].Position) * 0.5f;

	// Интерполируем атрибуты (цвет, uv)
	mid0.Color = (in[0].Color + in[1].Color) * 0.5f;
	mid1.Color = (in[1].Color + in[2].Color) * 0.5f;
	mid2.Color = (in[2].Color + in[0].Color) * 0.5f;

	mid0.UV = (in[0].UV + in[1].UV) * 0.5f;
	mid1.UV = (in[1].UV + in[2].UV) * 0.5f;
	mid2.UV = (in[2].UV + in[0].UV) * 0.5f;

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

// Шейдер для отображения текстуры на весь экран
float4 psTexture(const VertexOutput& in, ConstantBuffer /*cb*/)
{
	// Здесь мы ожидаем, что текстура будет передана через глобальную переменную или через захват лямбды
	// Для простоты используем глобальную текстуру (но в демо мы передадим её через захват лямбды)
	// В реальном коде можно передавать через uniform.
	return float4(1, 0, 1, 1); // заглушка, будет заменено
}

void CreateCube(VertexBuffer& vb, IndexBuffer& ib)
{
    // Координаты вершин куба (от -1 до 1)
    float3 corners[8] = {
        float3(-1, -1, -1), float3( 1, -1, -1), float3( 1,  1, -1), float3(-1,  1, -1),
        float3(-1, -1,  1), float3( 1, -1,  1), float3( 1,  1,  1), float3(-1,  1,  1)
    };

    // Цвета для каждой грани (R, G, B, A)
    float4 colors[6] = {
        float4(1, 0, 0, 1), // +X (красный)
        float4(0, 1, 0, 1), // -X (зелёный)
        float4(0, 0, 1, 1), // +Y (синий)
        float4(1, 1, 0, 1), // -Y (жёлтый)
        float4(0, 1, 1, 1), // +Z (голубой)
        float4(1, 0, 1, 1)  // -Z (фиолетовый)
    };

    // Определяем грани: для каждой грани задаём 4 индекса из corners[]
    int faces[6][4] = {
        {1, 5, 6, 2}, // +X
        {0, 3, 7, 4}, // -X
        {3, 2, 6, 7}, // +Y
        {0, 4, 5, 1}, // -Y
        {4, 7, 6, 5}, // +Z
        {0, 1, 2, 3}  // -Z
    };

    for (int f = 0; f < 6; ++f) {
        int idx[4] = { faces[f][0], faces[f][1], faces[f][2], faces[f][3] };
        float4 col = colors[f];

        int start = (int)vb.Size();
        vb.Add({ corners[idx[0]], col, float2(0, 0) });
        vb.Add({ corners[idx[1]], col, float2(1, 0) });
        vb.Add({ corners[idx[2]], col, float2(1, 1) });
        vb.Add({ corners[idx[3]], col, float2(0, 1) });

        // Два треугольника (0-1-2 и 0-2-3)
        ib.Add(start);
        ib.Add(start + 1);
        ib.Add(start + 2);
        ib.Add(start);
        ib.Add(start + 2);
        ib.Add(start + 3);
    }
}

void CreateSphere(VertexBuffer& vb, IndexBuffer& ib, float radius, int sliceCount, int stackCount)
{
	vb.Clear();
	ib.Clear();

	// Вершины
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

			float3 pos(radius * sinPhi * cosTheta, radius * cosPhi, radius * sinPhi * sinTheta);

			// Нормаль (нормализованная позиция)
			float3 normal = normalize(pos);

			// Цвет на основе нормали
			float4 color((normal.x + 1.0f) * 0.5f, (normal.y + 1.0f) * 0.5f, (normal.z + 1.0f) * 0.5f, 1.0f);

			// UV (для текстур)
			float2 uv((float)slice / sliceCount, (float)stack / stackCount);

			vb.Add({pos, color, uv});
		}
	}

	// Индексы
	for (int stack = 0; stack < stackCount; ++stack)
	{
		for (int slice = 0; slice < sliceCount; ++slice)
		{
			int first = stack * (sliceCount + 1) + slice;
			int second = first + 1;
			int third = first + (sliceCount + 1);
			int fourth = third + 1;

			// Два треугольника на квад
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
	CreateSphere(vb, ib, 1.0f, 64, 32);

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
	ctxLeft->SetPixelShader(psColor);
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
	ctxRight->SetPixelShader(psColor);
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
		float4x4 modelLeft = rotationY(angle);
		float aspect = 400.0f / 600.0f;
		float4x4 proj = perspectiveLH(3.14159f / 4.0f, aspect, 0.1f, 100.0f);
		TransformCB cbLeft;
		cbLeft.wvp = proj * view * modelLeft;
		ConstantBuffer cbLeftBuf(&cbLeft, sizeof(cbLeft));

		float4x4 modelRight = rotationY(angle);
		TransformCB cbRight;
		cbRight.wvp = proj * view * modelRight;
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
