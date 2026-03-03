#define SOFTX_STATIC
#include <Windows.h>
#include <SoftX/SoftX.h>
#include <thread>
#include <vector>
#include <string>

#pragma comment(lib, "SoftX.lib")

using namespace SoftX;

bool g_running = true;

// Структура константного буфера (объединённая)
struct LightCB
{
	float4x4 wvp;
	float4 lightDir;   // направление света (нормализованное)
	float4 lightColor; // цвет света
	float4 ambient;	   // фоновое освещение
	float4 eyePos;	   // позиция камеры (для блеска, пока не используется)
};

// Прототипы шейдеров
VertexOutput vsTransform(const VertexInput& in, ConstantBuffer cb);
void GSSplitTriangle(const VertexOutput in[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices);
float4 psLight(const VertexOutput& in, ConstantBuffer cb);
float4 psCombine(const VertexOutput& in, ConstantBuffer cb, const TextureRGBA32F* texLeft,
				 const TextureRGBA32F* texRight);

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

// Вершинный шейдер
VertexOutput vsTransform(const VertexInput& in, ConstantBuffer cb)
{
	const LightCB* data = (const LightCB*)cb.Data();
	VertexOutput out;
	out.Position = data->wvp * float4(in.Position.x, in.Position.y, in.Position.z, 1.0f);
	out.Normal = in.Normal; // в мировых координатах (объект не масштабируется)
	out.Color = in.Color;
	out.UV = in.UV;
	return out;
}

// Геометрический шейдер – разбивает треугольник на 4 меньших
void GSSplitTriangle(const VertexOutput in[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices)
{
	VertexOutput mid0, mid1, mid2;
#define LERP(vertex, field, i0, i1) vertex.field = (in[i0].field + in[i1].field) * 0.5f

	LERP(mid0, Position, 0, 1);
	LERP(mid1, Position, 1, 2);
	LERP(mid2, Position, 2, 0);

	LERP(mid0, Normal, 0, 1);
	LERP(mid1, Normal, 1, 2);
	LERP(mid2, Normal, 2, 0);

	LERP(mid0, Color, 0, 1);
	LERP(mid1, Color, 1, 2);
	LERP(mid2, Color, 2, 0);

	LERP(mid0, UV, 0, 1);
	LERP(mid1, UV, 1, 2);
	LERP(mid2, UV, 2, 0);
#undef LERP

	int base = (int)outVerts.size();
	outVerts.push_back(in[0]);
	outVerts.push_back(mid0);
	outVerts.push_back(in[1]);
	outVerts.push_back(mid1);
	outVerts.push_back(in[2]);
	outVerts.push_back(mid2);

	// Треугольник 1 (in0, mid0, mid2)
	outIndices.push_back(base + 0);
	outIndices.push_back(base + 1);
	outIndices.push_back(base + 5);
	// Треугольник 2 (mid0, in1, mid1)
	outIndices.push_back(base + 1);
	outIndices.push_back(base + 2);
	outIndices.push_back(base + 3);
	// Треугольник 3 (mid2, mid1, in2)
	outIndices.push_back(base + 5);
	outIndices.push_back(base + 3);
	outIndices.push_back(base + 4);
	// Треугольник 4 (mid0, mid1, mid2)
	outIndices.push_back(base + 1);
	outIndices.push_back(base + 3);
	outIndices.push_back(base + 5);
}

// Пиксельный шейдер с диффузным освещением
float4 psLight(const VertexOutput& in, ConstantBuffer cb)
{
	const LightCB* light = (const LightCB*)cb.Data();
	float3 N = normalize(in.Normal);
	float3 L = normalize(light->lightDir.xyz());
	float diff = std::max(0.0f, dot(N, L));
	float3 final = (light->ambient.xyz() + diff * light->lightColor.xyz()) * in.Color.xyz();
	return float4(final, in.Color.w);
}

// Шейдер для комбинирования двух текстур на экране
float4 psCombine(const VertexOutput& in, ConstantBuffer cb, const TextureRGBA32F* texLeft,
				 const TextureRGBA32F* texRight)
{
	float2 uv = in.UV;
	if (uv.x < 0.5f)
	{
		float2 uvLeft(uv.x * 2.0f, uv.y);
		return texLeft->sample(uvLeft);
	}
	else
	{
		float2 uvRight((uv.x - 0.5f) * 2.0f, uv.y);
		return texRight->sample(uvRight);
	}
}

// Создание сферы
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

int main()
{
	HINSTANCE hInstance = GetModuleHandle(nullptr);
	const wchar_t CLASS_NAME[] = L"SoftXDemo";
	WNDCLASS wc = {};
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	HWND hWnd = CreateWindowEx(0, CLASS_NAME, L"SoftX Demo", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
							   CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return -1;
	ShowWindow(hWnd, SW_SHOW);

	PresentParameters pp;
	pp.BackBufferSize = int2(800, 600);
	pp.hDeviceWindow = hWnd;
	pp.Windowed = true;

	Device device(pp);
	auto& immediateCtx = device.GetImmediateContext();

	// Ресурсы сферы
	VertexBuffer sphereVB;
	IndexBuffer sphereIB;
	CreateSphere(sphereVB, sphereIB, 1.0f, 16, 8); // более детальная сфера

	// Два рендертаргета
	RenderTargetTexture rtLeft(int2(400, 600));
	RenderTargetTexture rtRight(int2(400, 600));

	// Создаём отложенные контексты
	auto ctxLeft = device.CreateDeferredContext();
	auto ctxRight = device.CreateDeferredContext();

	// Настраиваем левый контекст (обычная сфера)
	ctxLeft->SetRenderTarget(&rtLeft, true);
	ctxLeft->SetViewport(Viewport(0, 0, 400, 600));
	ctxLeft->SetVertexShader(vsTransform);
	ctxLeft->SetPixelShader(psLight);
	ctxLeft->SetVertexBuffer(sphereVB);
	ctxLeft->SetIndexBuffer(sphereIB);
	ctxLeft->SetCullMode(CullMode::Back);
	ctxLeft->SetFillMode(FillMode::Solid);
	ctxLeft->SetTileRenderingState(true);
	ctxLeft->SetTileSize(16);

	// Настраиваем правый контекст (с геометрическим шейдером разбиения)
	ctxRight->SetRenderTarget(&rtRight, true);
	ctxRight->SetViewport(Viewport(0, 0, 400, 600));
	ctxRight->SetVertexShader(vsTransform);
	//ctxRight->SetGeometryShader(GSSplitTriangle);
	ctxRight->SetPixelShader(psLight);
	ctxRight->SetVertexBuffer(sphereVB);
	ctxRight->SetIndexBuffer(sphereIB);
	ctxRight->SetCullMode(CullMode::Back);
	ctxRight->SetFillMode(FillMode::Solid);
	ctxRight->SetTileRenderingState(true);
	ctxRight->SetTileSize(16);

	// Переменные анимации
	float sphereAngle = 0.0f; // вращение сфер
	float lightAngle = 0.0f;  // угол источника света
	LARGE_INTEGER freq, prevTime, fpsLastTime;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&prevTime);
	QueryPerformanceCounter(&fpsLastTime);
	int frameCount = 0;
	float fps = 0.0f;

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
		float deltaTime = float(double(currentTime.QuadPart - prevTime.QuadPart) / freq.QuadPart);
		prevTime = currentTime;

		// Обновляем анимацию
		sphereAngle += deltaTime * 0.5f; // пол-оборота в секунду
		lightAngle += deltaTime * 0.8f;	 // свет вращается чуть быстрее

		// Матрицы
		float3 eye(0, 0, -50);
		float3 target(0, 0, 0);
		float3 up(0, 1, 0);
		float4x4 view = lookAtLH(eye, target, up);
		float aspect = 400.0f / 600.0f;
		float4x4 proj = perspectiveLH(DegToRad(40.0f), aspect, 0.1f, 100.0f);

		// Левая сфера (вращается)
		float4x4 modelLeft = rotationY(sphereAngle) * rotationX(sphereAngle * 0.3f);
		// Правая сфера (тоже вращается, но геометрически разбита)
		float4x4 modelRight = rotationY(sphereAngle) * rotationX(sphereAngle * 0.3f);

		// Источник света движется по окружности в горизонтальной плоскости на высоте 2
		float3 lightPos(3.0f * cosf(lightAngle), 2.0f, 3.0f * sinf(lightAngle));
		float3 lightDir = normalize(lightPos - eye); // направление от камеры? Нет, от источника к объекту, но для
													 // диффузного нужно направление на источник.
		// Лучше направить свет из позиции источника на центр сцены:
		float3 lightDirToScene = normalize(target - lightPos); // направление от источника к центру (объекту)
		// Однако для диффузного освещения нам нужно направление от поверхности к источнику, поэтому L =
		// normalize(lightPos - fragPos). Но мы используем единое направление для всей сцены (направленный свет).
		// Сделаем направленный свет с вращающимся направлением. Для эффектности используем направленный свет, который
		// вращается вокруг вертикальной оси.
		float3 lightDirGlobal = normalize(float3(cosf(lightAngle), 1.0f, sinf(lightAngle))); // пучок под углом

		// Цвет света плавно меняется
		float4 lightColor(0.8f + 0.5f * sinf(lightAngle * 2.0f), 0.8f + 0.5f * sinf(lightAngle * 2.0f + 2.0f),
						  0.8f + 0.5f * sinf(lightAngle * 2.0f + 4.0f), 1.0f);

		// Константный буфер для левой сферы
		LightCB cbLeft;
		cbLeft.wvp = proj * view * modelLeft;
		cbLeft.lightDir = float4(lightDirGlobal.x, lightDirGlobal.y, lightDirGlobal.z, 0.0f);
		cbLeft.lightColor = lightColor;
		cbLeft.ambient = float4(0.2f, 0.2f, 0.2f, 1.0f);
		ConstantBuffer cbLeftBuf(&cbLeft, sizeof(cbLeft));

		// Константный буфер для правой сферы
		LightCB cbRight;
		cbRight.wvp = proj * view * modelRight;
		cbRight.lightDir = float4(lightDirGlobal.x, lightDirGlobal.y, lightDirGlobal.z, 0.0f);
		cbRight.lightColor = lightColor;
		cbRight.ambient = float4(0.2f, 0.2f, 0.2f, 1.0f);
		ConstantBuffer cbRightBuf(&cbRight, sizeof(cbRight));

		// Параллельный рендеринг в два контекста
		std::thread t1([&]() {
			ctxLeft->SetConstantBuffer(cbLeftBuf);
			ctxLeft->Clear(float4(0.1f, 0.1f, 0.1f, 1.0f));
			ctxLeft->ClearDepth(1.0f);
			ctxLeft->DrawIndexed();
		});
		std::thread t2([&]() {
			ctxRight->SetConstantBuffer(cbRightBuf);
			ctxRight->Clear(float4(0.1f, 0.1f, 0.1f, 1.0f));
			ctxRight->ClearDepth(1.0f);
			ctxRight->DrawIndexed();
		});
		t1.join();
		t2.join();

		// Вывод на экран: комбинируем левую и правую текстуры
		immediateCtx.SetRenderTarget(&device.GetBackBuffer(), false);
		immediateCtx.Clear(float4(0, 0, 0, 1));
		immediateCtx.ClearDepth(1.0f);

		// Лямбда-шейдер, захватывающий текстуры
		auto psCombineFunc = [&](const VertexOutput& in, ConstantBuffer) -> float4 {
			return psCombine(in, ConstantBuffer(), &rtLeft.texture(), &rtRight.texture());
		};
		immediateCtx.SetPixelShader(psCombineFunc);
		immediateCtx.SetTileRenderingState(true);
		immediateCtx.SetTileSize(128);
		immediateCtx.DrawFullScreenQuad();

		device.Present();

		// Обновление FPS
		frameCount++;
		double elapsedFPS = double(currentTime.QuadPart - fpsLastTime.QuadPart) / freq.QuadPart;
		if (elapsedFPS >= 1.0)
		{
			fps = frameCount / (float)elapsedFPS;
			wchar_t title[256];
			swprintf(title, 256, L"SoftX Demo - FPS: %.1f", fps);
			SetWindowTextW(hWnd, title);
			frameCount = 0;
			fpsLastTime = currentTime;
		}
	}

	return 0;
}