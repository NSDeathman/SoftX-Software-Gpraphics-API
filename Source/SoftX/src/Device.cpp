#include "pch.h"
#include <SoftX/SoftX.h>  // или соответствующие заголовки
#include <atomic>

SOFTX_BEGIN

//  онструктор
Device::Device(const PresentParameters& params)
    : m_params(params)
    , m_backBuffer(params.BackBufferSize)
    , m_depthBuffer(params.BackBufferSize)
    , m_threadPool(std::make_unique<ThreadPool>(std::thread::hardware_concurrency()))
{
}

// —еттер/геттер дл€ контекста
void Device::SetDeviceContext(const DeviceContext& ctx)
{
    m_DeviceContext = ctx;
}

DeviceContext Device::GetDeviceContext() const
{
    return m_DeviceContext;
}

void Device::SetVertexBuffer(const VertexBuffer& buffer)
{
	m_DeviceContext.SetVertexBuffer(buffer);
}

void Device::SetIndexBuffer(const IndexBuffer& buffer)
{
	m_DeviceContext.SetIndexBuffer(buffer);
}

void Device::SetConstantBuffer(ConstantBuffer cbuffer)
{
	m_DeviceContext.SetConstantBuffer(cbuffer);
}

// ќчистка цветом: используем рендертаргет из контекста или backbuffer по умолчанию
void Device::Clear(const float4& color)
{
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (rt)
        rt->clear(color);
    else
        m_backBuffer.clear(color);
}

void Device::ClearDepth(float depth)
{
    m_depthBuffer.clear(depth);
}

Framebuffer& Device::GetBackBuffer()
{
    return m_backBuffer;
}

PresentParameters& Device::GetPresentParams()
{
    return m_params;
}

// ¬спомогательный метод дл€ отрисовки одного тайла (используетс€ в DrawFullScreenQuad)
void Device::renderTileQuad(int tileIndex)
{
    const Tile& tile = m_tiles[tileIndex];
    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) return;  // если рендертаргет не задан, выходим

    int w = rt->width();
    int h = rt->height();

    VertexOutput input = {};
    auto ps = m_DeviceContext.GetPixelShader();
    auto cb = m_DeviceContext.GetConstantBuffer();

    for (int y = tile.min.y; y <= tile.max.y; ++y)
    {
        float v = (float)y / (h - 1);
        for (int x = tile.min.x; x <= tile.max.x; ++x)
        {
            float u = (float)x / (w - 1);
            input.UV = float2(u, v);
            float4 color = ps(input, cb);
            rt->set_pixel(int2(x, y), color);
        }
    }
}

void Device::DrawFullScreenQuad()
{
    auto ps = m_DeviceContext.GetPixelShader();
    if (!ps) return;

    IRenderTarget* rt = m_DeviceContext.GetRenderTarget();
    if (!rt) rt = &m_backBuffer;  // по умолчанию используем backbuffer

    int w = rt->width();
    int h = rt->height();

    // ѕостроить тайлы дл€ текущего размера экрана
    buildTiles(w, h); // использует m_tileSize из контекста? надо брать из контекста
    // исправим buildTiles, чтобы он использовал размер из контекста
    // пока оставим как есть, но позже нужно будет везде использовать контекст

    int numTiles = (int)m_tiles.size();
    std::atomic<int> tileIndex(0);

    auto worker = [this, &tileIndex, numTiles]() {
        while (true)
        {
            int idx = tileIndex.fetch_add(1);
            if (idx >= numTiles) break;
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
	std::string err;
	bool rslt = m_DeviceContext.Validate(&err);
	if (!rslt)
	{
		printf("%s", err.c_str());
		return;
	}

    // ѕолучаем все необходимые данные из контекста
    auto vs = m_DeviceContext.GetVertexShader();
    auto ps = m_DeviceContext.GetPixelShader();
    auto vb = m_DeviceContext.GetVertexBuffer();
    auto ib = m_DeviceContext.GetIndexBuffer();
    auto cb = m_DeviceContext.GetConstantBuffer();
    auto rt = m_DeviceContext.GetRenderTarget();
    auto fillMode = m_DeviceContext.GetFillMode();
    auto cullMode = m_DeviceContext.GetCullMode();
    auto tiledEnabled = m_DeviceContext.GetTileRenderingState();
    auto tileSize = m_DeviceContext.GetTileSize();  // размер тайла дл€ биннинга

    // ќчищаем временные массивы
    m_transformedVerts.clear();
    m_triangles.clear();

    // “рансформируем все вершины, которые используютс€ в индексах
    std::vector<bool> vertexProcessed(vb.Size(), false);
    for (uint32_t i = startIndex; i < startIndex + indexCount; ++i)
    {
        uint32_t idx = ib.GetByIndex(i);
        if (!vertexProcessed[idx])
        {
            vertexProcessed[idx] = true;
            VertexOutput out = vs(vb.GetByIndex(idx), cb);
            out.Position = ClipToScreen(out.Position); // использует текущий viewport из контекста? ClipToScreen должен брать viewport из контекста
            if (m_transformedVerts.size() <= idx)
                m_transformedVerts.resize(idx + 1);
            m_transformedVerts[idx] = out;
        }
    }

    // —обираем треугольники
    for (uint32_t i = startIndex; i < startIndex + indexCount; i += 3)
    {
        if (i + 2 >= startIndex + indexCount) break;
        uint32_t i0 = ib.GetByIndex(i);
        uint32_t i1 = ib.GetByIndex(i + 1);
        uint32_t i2 = ib.GetByIndex(i + 2);
        m_triangles.push_back({(int)i0, (int)i1, (int)i2});
    }

    if (fillMode == FillMode::Solid)
    {
        if (tiledEnabled)
        {
            buildTiles(rt->width(), rt->height()); // передаЄм размеры рендертаргета
            // ¬ buildTiles нужно будет использовать tileSize из контекста
            // Ќо пока оставим как есть, использу€ сохранЄнный m_tileSize, который нужно синхронизировать
            binTriangles(m_transformedVerts, m_triangles);
            renderTilesMultithreaded();
        }
        else
        {
            // ѕоследовательный рендеринг
            for (const auto& tri : m_triangles)
            {
                RasterizeTriangleSSE(m_transformedVerts[tri.x], m_transformedVerts[tri.y], m_transformedVerts[tri.z]);
            }
        }
    }
    else if (fillMode == FillMode::Wireframe)
    {
        float4 wireColor(1.0f, 1.0f, 1.0f, 1.0f);
        for (const auto& tri : m_triangles)
        {
            const auto& v0 = m_transformedVerts[tri.x];
            const auto& v1 = m_transformedVerts[tri.y];
            const auto& v2 = m_transformedVerts[tri.z];
            DrawLine((int)round(v0.Position.x), (int)round(v0.Position.y),
                     (int)round(v1.Position.x), (int)round(v1.Position.y),
                     v0.Position.z, v1.Position.z, wireColor);
            DrawLine((int)round(v1.Position.x), (int)round(v1.Position.y),
                     (int)round(v2.Position.x), (int)round(v2.Position.y),
                     v1.Position.z, v2.Position.z, wireColor);
            DrawLine((int)round(v2.Position.x), (int)round(v2.Position.y),
                     (int)round(v0.Position.x), (int)round(v0.Position.y),
                     v2.Position.z, v0.Position.z, wireColor);
        }
    }
    else if (fillMode == FillMode::Point)
    {
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
    // »спользуем все индексы из индексного буфера, хран€щегос€ в контексте
    uint32_t count = (uint32_t)m_DeviceContext.GetIndexBuffer().Size();
    DrawIndexed(count, 0);
}

void Device::Present()
{
    HDC hdc = GetDC(m_params.hDeviceWindow);
    if (hdc) {
        RECT clientRect;
        GetClientRect(m_params.hDeviceWindow, &clientRect);
        int2 dstSize(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
        m_backBuffer.present(hdc, int2(0, 0), dstSize);
        ReleaseDC(m_params.hDeviceWindow, hdc);
    }
}

SOFTX_END
