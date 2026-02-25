#include "Device.h"

Device::Device(const PresentParameters& params)
    : m_params(params)
    , m_backBuffer(params.BackBufferSize)
    , m_depthBuffer(params.BackBufferSize)
    , m_currentRT(&m_backBuffer)
    , m_pixelShader(nullptr)
    , m_vertexShader(nullptr)
    , m_constant_buffer(nullptr)
    , m_constant_buffer_size(0)
    , m_cullMode(CullMode::Back)
	, m_fillMode(FillMode::Solid)
    , m_viewport(0, 0, (float)params.BackBufferSize.x, (float)params.BackBufferSize.y, 0, 1)
	, m_threadPool(std::make_unique<ThreadPool>(std::thread::hardware_concurrency()))
{
}

void Device::DrawFullScreenQuad()
{
	if (!m_pixelShader)
		return;

	int w = m_currentRT->width();
	int h = m_currentRT->height();

	VertexOutput Input = {};

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			Input.UV = float2((float)x / (w - 1), (float)y / (h - 1));
			float4 color = m_pixelShader(Input, m_constant_buffer);
			m_currentRT->set_pixel(int2(x, y), color);
		}
	}
}

void Device::DrawIndexed(uint32_t indexCount, uint32_t startIndex)
{
	if (!m_vertexShader || !m_pixelShader || m_vertexBuffer.empty() || m_indexBuffer.empty())
		return;

	// Очищаем временные массивы
	m_transformedVerts.clear();
	m_triangles.clear();

	// Трансформируем все вершины, которые используются в индексах
	// (для простоты трансформируем все уникальные индексы)
	std::vector<bool> vertexProcessed(m_vertexBuffer.size(), false);
	for (uint32_t i = startIndex; i < startIndex + indexCount; ++i)
	{
		uint32_t idx = m_indexBuffer[i];
		if (!vertexProcessed[idx])
		{
			vertexProcessed[idx] = true;
			VertexOutput out = m_vertexShader(m_vertexBuffer[idx], m_constant_buffer);
			out.Position = ClipToScreen(out.Position);
			// Сохраняем в массив, позиция по idx
			if (m_transformedVerts.size() <= idx)
				m_transformedVerts.resize(idx + 1);
			m_transformedVerts[idx] = out;
		}
	}

	// Собираем треугольники
	for (uint32_t i = startIndex; i < startIndex + indexCount; i += 3)
	{
		if (i + 2 >= startIndex + indexCount)
			break;
		uint32_t i0 = m_indexBuffer[i];
		uint32_t i1 = m_indexBuffer[i + 1];
		uint32_t i2 = m_indexBuffer[i + 2];
		m_triangles.push_back({(int)i0, (int)i1, (int)i2});
	}

	if (m_fillMode == FillMode::Solid)
	{
		if (m_tiledRendering)
		{
			LARGE_INTEGER freq, t1, t2;
			QueryPerformanceFrequency(&freq);

			QueryPerformanceCounter(&t1);
			buildTiles(m_currentRT->width(), m_currentRT->height());
			QueryPerformanceCounter(&t2);
			double buildTime = double(t2.QuadPart - t1.QuadPart) / freq.QuadPart;

			QueryPerformanceCounter(&t1);
			binTriangles(m_transformedVerts, m_triangles);
			QueryPerformanceCounter(&t2);
			double binTime = double(t2.QuadPart - t1.QuadPart) / freq.QuadPart;

			QueryPerformanceCounter(&t1);
			renderTilesMultithreaded();
			QueryPerformanceCounter(&t2);
			double renderTime = double(t2.QuadPart - t1.QuadPart) / freq.QuadPart;

			char buf[256];
			sprintf_s(buf, "Build: %.3f ms, Bin: %.3f ms, Render: %.3f ms\n", buildTime * 1000, binTime * 1000,
					  renderTime * 1000);
			OutputDebugStringA(buf);
		}
		else
		{
			// Последовательный рендеринг
			for (const auto& tri : m_triangles)
			{
				RasterizeTriangleSSE(m_transformedVerts[tri.x], m_transformedVerts[tri.y], m_transformedVerts[tri.z]);
			}
		}
	}
	else if (m_fillMode == FillMode::Wireframe)
	{
		// Рисуем рёбра треугольников белым цветом (можно изменить)
		float4 wireColor(1.0f, 1.0f, 1.0f, 1.0f);
		for (const auto& tri : m_triangles)
		{
			const auto& v0 = m_transformedVerts[tri.x];
			const auto& v1 = m_transformedVerts[tri.y];
			const auto& v2 = m_transformedVerts[tri.z];
			DrawLine((int)round(v0.Position.x), (int)round(v0.Position.y), (int)round(v1.Position.x),
					 (int)round(v1.Position.y), v0.Position.z, v1.Position.z, wireColor);
			DrawLine((int)round(v1.Position.x), (int)round(v1.Position.y), (int)round(v2.Position.x),
					 (int)round(v2.Position.y), v1.Position.z, v2.Position.z, wireColor);
			DrawLine((int)round(v2.Position.x), (int)round(v2.Position.y), (int)round(v0.Position.x),
					 (int)round(v0.Position.y), v2.Position.z, v0.Position.z, wireColor);
		}
	}
	else if (m_fillMode == FillMode::Point)
	{
		// Рисуем вершины (каждую один раз)
		std::vector<bool> drawn(m_transformedVerts.size(), false);
		for (const auto& tri : m_triangles)
		{
			for (int idx : {tri.x, tri.y, tri.z})
			{
				if (!drawn[idx])
				{
					drawn[idx] = true;
					const auto& v = m_transformedVerts[idx];
					DrawPoint((int)round(v.Position.x), (int)round(v.Position.y), v.Position.z, v.Color);
				}
			}
		}
	}
}


void Device::Present() {
    HDC hdc = GetDC(m_params.hDeviceWindow);
    if (hdc) {
        // Получаем текущий размер клиентской области окна (может меняться)
        RECT clientRect;
        GetClientRect(m_params.hDeviceWindow, &clientRect);
        int2 dstSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);

        // Выводим задний буфер в окно (можно масштабировать, но пока просто копируем)
        m_backBuffer.present(hdc, int2(0, 0), dstSize);

        ReleaseDC(m_params.hDeviceWindow, hdc);
    }
}
