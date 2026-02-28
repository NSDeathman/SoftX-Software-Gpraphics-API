#pragma once

#include <windows.h>
#include <functional>

#include "LibInternal.h"
#include "ThreadPool.h"
#include "DeviceContext.h"

SOFTX_BEGIN

class SOFTX_API Device {
public:
    Device(const PresentParameters& params);
    ~Device() = default;

	void SetDeviceContext(const DeviceContext& ctx);
	DeviceContext GetDeviceContext() const;

	void SetVertexBuffer(const VertexBuffer& buffer);
	void SetIndexBuffer(const IndexBuffer& buffer);
	void SetConstantBuffer(ConstantBuffer cbuffer);

    // Очистка заднего буфера цветом
    void Clear(const float4& color);
    void ClearDepth(float depth);

    // Рисует полноэкранный четырёхугольник, выполняя пиксельный шейдер для каждого пикселя
	void DrawFullScreenQuad();
    void DrawIndexed(uint32_t indexCount, uint32_t startIndex);
    void DrawIndexed();

    float4 ClipToScreen(const float4& clipPos) const;
    void DrawPoint(int x, int y, float z, const float4& color);
	void DrawLine(int x0, int y0, int x1, int y1, float z0, float z1, const float4& color);
	void RasterizeTriangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2);
	void RasterizeTriangleSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2);

    // Презентация: копирует задний буфер в окно
    void Present();

    // Доступ к заднему буферу для рисования (прямое манипулирование пикселями)
    Framebuffer& GetBackBuffer();

    // Геттеры для параметров
    PresentParameters& GetPresentParams();

private:
    PresentParameters m_params;

    Framebuffer m_backBuffer;
    DepthBuffer m_depthBuffer;

	DeviceContext m_DeviceContext;

	bool m_tiledRendering;
	int m_tileSize;
	std::vector<Tile> m_tiles;
	std::vector<VertexOutput> m_transformedVerts;
	std::vector<int3> m_triangles;
	std::unique_ptr<ThreadPool> m_threadPool;

	// Методы для тайлового рендера
	void buildTiles(int width, int height);
	void binTriangles(const std::vector<VertexOutput>& transformedVerts, const std::vector<int3>& triangles);
	void renderTilesMultithreaded();
	void renderTilesSingleThreaded();
	void RasterizeTriangleTile(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax);
	void RasterizeTriangleTileSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax);
	void renderTile(int tileIndex);
	void renderTileQuad(int tileIndex);
};

SOFTX_END
