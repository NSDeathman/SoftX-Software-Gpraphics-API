#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <memory>
#include <vector>
#include <cmath>

#include <SoftX/SoftX.h>
#pragma comment(lib, "SoftX.lib")

using namespace SoftX;
using namespace AfterMath;

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

HWND g_hWnd = nullptr;
Device* g_device = nullptr;

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
//  Constant buffer
// ============================================================
struct ConstantBufferData
{
	float4x4 modelViewProjection;
};

// ============================================================
//  UV Checker — глобальная текстура, живёт всё время программы.
//  Передаём указатель на неё через глобальную переменную,
//  пока не реализован полноценный механизм текстурных слотов.
// ============================================================
static constexpr int CHECKER_SIZE = 512;
static constexpr int CHECKER_CELLS = 8;

static const float4 CHECKER_PALETTE[4] = {
	float4(0.90f, 0.20f, 0.20f, 1.0f),
	float4(0.20f, 0.60f, 0.90f, 1.0f),
	float4(0.20f, 0.80f, 0.30f, 1.0f),
	float4(0.95f, 0.80f, 0.10f, 1.0f),
};

TextureRGBA32F CreateUVCheckerTexture()
{
	auto tex = new TextureRGBA32F(uint2(CHECKER_SIZE, CHECKER_SIZE));
	const int cellSize = CHECKER_SIZE / CHECKER_CELLS;

	for (int y = 0; y < CHECKER_SIZE; ++y)
	{
		for (int x = 0; x < CHECKER_SIZE; ++x)
		{
			int cellX = x / cellSize;
			int cellY = y / cellSize;
			float fx = (float)(x % cellSize) / (float)cellSize;
			float fy = (float)(y % cellSize) / (float)cellSize;

			const float gridWidth = 0.08f;
			bool isGrid = (fx < gridWidth || fx > 1.0f - gridWidth || fy < gridWidth || fy > 1.0f - gridWidth);

			float4 color;
			if (isGrid)
			{
				color = float4(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				int paletteIdx = (cellX + cellY) % 4;
				color = CHECKER_PALETTE[paletteIdx];
				float shade = 0.75f + 0.25f * (fx * 0.5f + fy * 0.5f);
				color.x *= shade;
				color.y *= shade;
				color.z *= shade;
			}

			__m128 c = _mm_set_ps(color.w, color.z, color.y, color.x);
			tex->StreamWrite(int2(x, y), c);
		}
	}
	return *tex;
}

// ============================================================
//  Shaders
// ============================================================
VertexOutput TransformVS(const VertexInput& input, ConstantBuffer cb, const TextureTable& tex)
{
	const ConstantBufferData* data = reinterpret_cast<const ConstantBufferData*>(cb.Data());
	VertexOutput output;
	float4 objectPos(input.Position.x, input.Position.y, input.Position.z, 1.0f);
	output.Position = objectPos * data->modelViewProjection;
	output.Color = input.Color;
	output.Normal = input.Normal;
	output.UV = input.UV;
	return output;
}

float4 UVCheckerPSNearest(const VertexOutput& input, ConstantBuffer cb, const TextureTable& tex)
{
	return tex[1].Sample(input.UV);
}

float4 UVCheckerPSBillinear(const VertexOutput& input, ConstantBuffer cb, const TextureTable& tex)
{
	if (tex[0].IsEmpty())
		return float4(1.0f, 0.0f, 1.0f, 1.0f); // маджента — нет текстуры

	return tex[0].Sample(input.UV);
}

// ============================================================
//  Geometry
// ============================================================
void CreateSphere(VertexBuffer& vb, IndexBuffer& ib, float radius, int slices, int stacks)
{
	std::vector<VertexInput> vertices;
    std::vector<uint> indices;

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

	vb = VertexBuffer(std::move(vertices));
	ib = IndexBuffer(std::move(indices));
}

void CreateCube(VertexBuffer& vb, IndexBuffer& ib, float size = 1.0f)
{
	std::vector<VertexInput> vertices;
    std::vector<uint> indices;

	float half = size * 0.5f;
	float3 corners[8] = {float3(-half, -half, -half), float3(half, -half, -half), float3(half, half, -half),
						 float3(-half, half, -half),  float3(-half, -half, half), float3(half, -half, half),
						 float3(half, half, half),	  float3(-half, half, half)};
	float3 normals[6] = {float3(0, 0, -1), float3(0, 0, 1),	 float3(0, -1, 0),
						 float3(0, 1, 0),  float3(-1, 0, 0), float3(1, 0, 0)};
	float4 colors[6] = {float4(1, 0, 0, 1), float4(0, 1, 0, 1), float4(0, 0, 1, 1),
						float4(1, 1, 0, 1), float4(1, 0, 1, 1), float4(0, 1, 1, 1)};
	int faceIndices[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {0, 4, 5, 1}, {3, 2, 6, 7}, {0, 3, 7, 4}, {1, 5, 6, 2}};
	float2 uvs[4] = {float2(0, 0), float2(1, 0), float2(1, 1), float2(0, 1)};

	for (int f = 0; f < 6; ++f)
	{
		int* idx = faceIndices[f];
		int start = (int)vertices.size();
		for (int v = 0; v < 4; ++v)
			vertices.push_back({corners[idx[v]], normals[f], colors[f], uvs[v]});

		// CCW winding — исправленный порядок индексов
		indices.push_back(start);
		indices.push_back(start + 2);
		indices.push_back(start + 1);
		indices.push_back(start);
		indices.push_back(start + 3);
		indices.push_back(start + 2);
	}

	vb = VertexBuffer(std::move(vertices));
	ib = IndexBuffer(std::move(indices));
}

// ============================================================
//  DrawFrame
// ============================================================
void DrawFrame()
{
	PROFILE_SCOPE("DrawFrame");

	DeviceContext& ctx = g_device->GetImmediateContext();

	// ── Матрицы ──────────────────────────────────────────────
	float aspect = (float)WINDOW_WIDTH * 0.5f / (float)WINDOW_HEIGHT;

	// Передаём aspect явно в perspective чтобы избежать сплющивания.
	// Если сигнатура твоей функции perspective(fovY, aspect) —
	// проверь что aspect применяется к X, а не Y.
	float4x4 projection = perspective(Constants::degrees_to_radians(30.0f), aspect);

	float3 eye(0.0f, 0.0f, -10.0f);
	float3 target(0.0f, 0.0f, 1.0f);
	float4x4 view = look_at(eye, target);

	static float angle = 0.0f;
	angle += 0.01f;

	float4x4 model = rotation_y(angle) * scaling(2.0f);
	float4x4 mvp = model * view * projection;

	ConstantBufferData cbData;
	cbData.modelViewProjection = mvp;
	ConstantBuffer mvpCB(&cbData, sizeof(cbData));

	// ── Очистка — один раз перед обоими проходами ────────────
	ctx.Clear(float4(0.0f, 0.15f, 0.25f, 1.0f));
	ctx.ClearDepth(1.0f);

	// ════════════════════════════════════════════════════════
	//  Pass 1 — левая половина: цвет вершин
	// ════════════════════════════════════════════════════════
	ctx.SetViewport(Viewport(0.0f, 0.0f, WINDOW_WIDTH / 2, WINDOW_HEIGHT, 0.0f, 1.0f));
	ctx.SetConstantBuffer(mvpCB);
	ctx.SetPixelShader(UVCheckerPSNearest);
	ctx.DrawIndexed();

	// ════════════════════════════════════════════════════════
	//  Pass 2 — правая половина: UV checker текстура
	// ════════════════════════════════════════════════════════
	ctx.SetViewport(Viewport((float)(WINDOW_WIDTH / 2), 0.0f, WINDOW_WIDTH / 2, WINDOW_HEIGHT, 0.0f, 1.0f));
	ctx.ClearDepth(1.0f);		  // сброс глубины — независимый проход
	ctx.SetConstantBuffer(mvpCB); // MVP тот же, cb для текстуры не нужен
	ctx.SetPixelShader(UVCheckerPSBillinear);
	ctx.DrawIndexed();

	// Восстанавливаем полный viewport на следующий кадр
	ctx.SetViewport(Viewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f));

	g_device->Present();
}

// ============================================================
//  WinMain
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = L"SoftXTestWindow";
	RegisterClassEx(&wc);

	RECT rc = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	g_hWnd = CreateWindowEx(0, L"SoftXTestWindow",
							L"SoftX | Left: Vertex Color  Right: UV Checker  [G] Sphere/Cube  [D] Depth func",
							WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
							nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
		return -1;

	ShowWindow(g_hWnd, nCmdShow);
	UpdateWindow(g_hWnd);

	TextureRGBA32F checkerTexture = CreateUVCheckerTexture();

	// ── SoftX Device ─────────────────────────────────────────
	PresentParameters params;
	params.BackBufferSize = uint2(WINDOW_WIDTH, WINDOW_HEIGHT);
	params.hDeviceWindow = g_hWnd;
	params.Windowed = true;

	Device device(params);
	g_device = &device;

	DeviceContext& ctx = g_device->GetDeviceContext();
	ctx.SetRenderTarget(&device.GetBackBuffer(), true);
	ctx.SetViewport(Viewport(0.0f, 0.0f, WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f, 1.0f));
	ctx.SetTexture(0, &checkerTexture, SamplerState{Filter::Bilinear, Wrap::Repeat, Wrap::Repeat});
	ctx.SetTexture(1, &checkerTexture, SamplerState{Filter::Nearest, Wrap::Repeat, Wrap::Repeat});

	VertexBuffer vb;
	IndexBuffer ib;
	CreateSphere(vb, ib, 1.0f, 64, 32);

	ctx.SetVertexBuffer(vb);
	ctx.SetIndexBuffer(ib);
	ctx.SetVertexShader(TransformVS);
	ctx.SetCullMode(CullMode::Back);
	ctx.SetTileSize(128);

	// ── Message loop ─────────────────────────────────────────
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
			DrawFrame();
		}
	}

	return (int)msg.wParam;
}

// ============================================================
//  WndProc
// ============================================================
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

		if (wParam == 'D' && g_device)
		{
			static int funcIndex = 0;
			funcIndex = (funcIndex + 1) % 8;
			ComparisonFunc funcs[8] = {ComparisonFunc::Never,		 ComparisonFunc::Less,	  ComparisonFunc::Equal,
									   ComparisonFunc::LessEqual,	 ComparisonFunc::Greater, ComparisonFunc::NotEqual,
									   ComparisonFunc::GreaterEqual, ComparisonFunc::Always};
			g_device->GetImmediateContext().SetDepthFunc(funcs[funcIndex]);
			char title[256];
			sprintf_s(title, "SoftX - Depth Func: %d", funcIndex);
			SetWindowTextA(g_hWnd, title);
		}

		if (wParam == 'G' && g_device)
		{
			static bool isSphere = true;
			isSphere = !isSphere;
			VertexBuffer vb;
			IndexBuffer ib;
			if (isSphere)
				CreateSphere(vb, ib, 1.0f, 64, 32);
			else
				CreateCube(vb, ib, 2.0f);
			DeviceContext& ctx = g_device->GetImmediateContext();
			ctx.SetVertexBuffer(vb);
			ctx.SetIndexBuffer(ib);
			SetWindowTextA(g_hWnd, isSphere ? "SoftX - Sphere" : "SoftX - Cube");
		}
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
