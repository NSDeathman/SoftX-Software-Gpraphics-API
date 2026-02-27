#include "pch.h"

SOFTX_BEGIN

Device::Device(const PresentParameters& params)
	: m_params(params)
	, m_backBuffer(params.BackBufferSize)
	, m_depthBuffer(params.BackBufferSize)
	, m_currentRT(&m_backBuffer)
    , m_pixelShader(nullptr)
    , m_vertexShader(nullptr)
    , m_cullMode(CullMode::Back)
	, m_fillMode(FillMode::Solid)
    , m_viewport(0, 0, (float)params.BackBufferSize.x, (float)params.BackBufferSize.y, 0, 1)
	, m_threadPool(std::make_unique<ThreadPool>(std::thread::hardware_concurrency()))
	, m_tiledRendering(true)
	, m_tileSize(64)
{
}

void Device::Clear(const float4& color) { m_currentRT->clear(color); };
void Device::ClearDepth(float depth) { m_depthBuffer.clear(depth); };

void Device::SetPixelShader(PixelShader shader) { m_pixelShader = std::move(shader); };
void Device::SetVertexShader(VertexShader shader) { m_vertexShader = std::move(shader); };

void Device::SetConstantBuffer(ConstantBuffer CBuffer) { m_constant_buffer = CBuffer; };

void Device::SetVertexBuffer(const VertexBuffer& buffer) { m_vertexBuffer = buffer; }
void Device::SetIndexBuffer(const IndexBuffer& buffer) { m_indexBuffer = buffer; }

void Device::SetRenderTarget(IRenderTarget* rt) { m_currentRT = rt ? rt : &m_backBuffer; }
IRenderTarget* Device::GetRenderTarget() const { return m_currentRT; }

void Device::SetViewport(const Viewport& vp) { m_viewport = vp; }
const Viewport& Device::GetViewport() const { return m_viewport; }

void Device::SetCullMode(CullMode mode) { m_cullMode = mode; }
CullMode Device::GetCullMode() const { return m_cullMode; }

void Device::SetFillMode(FillMode mode) { m_fillMode = mode; }
FillMode Device::GetFillMode() const { return m_fillMode; }

void Device::EnableTiledRendering(bool enable) { m_tiledRendering = enable; };
void Device::SetTileSize(int tileSize) { m_tileSize = tileSize; };
bool Device::IsTiledRenderingEnabled() const { return m_tiledRendering; };

Framebuffer& Device::GetBackBuffer() { return m_backBuffer; }

PresentParameters& Device::GetPresentParams() { return m_params; }

void Device::renderTileQuad(int tileIndex)
{
	const Tile& tile = m_tiles[tileIndex];
	int w = m_currentRT->width();
	int h = m_currentRT->height();

	VertexOutput input = {};

	for (int y = tile.min.y; y <= tile.max.y; ++y)
	{
		float v = (float)y / (h - 1);
		for (int x = tile.min.x; x <= tile.max.x; ++x)
		{
			float u = (float)x / (w - 1);
			input.UV = float2(u, v);
			float4 color = m_pixelShader(input, m_constant_buffer);
			m_currentRT->set_pixel(int2(x, y), color);
		}
	}
}

void Device::DrawFullScreenQuad()
{
	if (!m_pixelShader)
		return;

	int w = m_currentRT->width();
	int h = m_currentRT->height();

	// Построить тайлы для текущего размера экрана
	buildTiles(w, h); // использует m_tileSize (например, 64)

	int numTiles = (int)m_tiles.size();
	std::atomic<int> tileIndex(0);

	auto worker = [this, &tileIndex, numTiles]() {
		while (true)
		{
			int idx = tileIndex.fetch_add(1);
			if (idx >= numTiles)
				break;
			renderTileQuad(idx);
		}
	};

	int numThreads = (int)m_threadPool->threadCount();
	for (int i = 0; i < numThreads; ++i)
	{
		m_threadPool->enqueue(worker);
	}
	m_threadPool->wait();
}

void Device::DrawIndexed(uint32_t indexCount, uint32_t startIndex)
{
	if (!m_vertexShader || !m_pixelShader || m_vertexBuffer.IsEmpty() || m_indexBuffer.IsEmpty())
		return;

	// Очищаем временные массивы
	m_transformedVerts.clear();
	m_triangles.clear();

	// Трансформируем все вершины, которые используются в индексах
	// (для простоты трансформируем все уникальные индексы)
	std::vector<bool> vertexProcessed(m_vertexBuffer.Size(), false);
	for (uint32_t i = startIndex; i < startIndex + indexCount; ++i)
	{
		uint32_t idx = m_indexBuffer.GetByIndex(i);
		if (!vertexProcessed[idx])
		{
			vertexProcessed[idx] = true;
			VertexOutput out = m_vertexShader(m_vertexBuffer.GetByIndex(idx), m_constant_buffer);
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
		uint32_t i0 = m_indexBuffer.GetByIndex(i);
		uint32_t i1 = m_indexBuffer.GetByIndex(i + 1);
		uint32_t i2 = m_indexBuffer.GetByIndex(i + 2);
		m_triangles.push_back({(int)i0, (int)i1, (int)i2});
	}

	if (m_fillMode == FillMode::Solid)
	{
		if (m_tiledRendering)
		{
			buildTiles(m_currentRT->width(), m_currentRT->height());
			binTriangles(m_transformedVerts, m_triangles);
			renderTilesMultithreaded();
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

void Device::DrawIndexed() 
{ 
	DrawIndexed((uint32_t)m_indexBuffer.Size(), 0); 
};

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

SOFTX_END
