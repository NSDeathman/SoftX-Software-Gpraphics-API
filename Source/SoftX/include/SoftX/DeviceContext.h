#pragma once

#include "ThirdPartyIncluding.h"
#include "LibInternal.h"
#include "Types.h"
#include "RenderTargetInterface.h"

SOFTX_BEGIN

class SOFTX_API DeviceContext
{
  public:
	DeviceContext();
	~DeviceContext();

	DeviceContext(DeviceContext&&) = default;
	DeviceContext& operator=(DeviceContext&&) = default;

	// Сеттеры и геттеры для шейдеров
	void SetVertexShader(VertexShader shader);
	VertexShader GetVertexShader() const;

	void SetGeometryShader(GeometryShader shader);
	GeometryShader GetGeometryShader() const;

	void SetPixelShader(PixelShader shader);
	PixelShader GetPixelShader() const;

	// Сеттеры и геттеры для буферов
	void SetVertexBuffer(const VertexBuffer& buffer);
	VertexBuffer GetVertexBuffer() const;

	void SetIndexBuffer(const IndexBuffer& buffer);
	IndexBuffer GetIndexBuffer() const;

	void SetConstantBuffer(const ConstantBuffer& buffer);
	ConstantBuffer GetConstantBuffer() const;

	// Сеттеры и геттеры для рендертаргета
	void SetRenderTarget(IRenderTarget* target);
	void SetRenderTarget(IRenderTarget* target, bool createDepthBuffer = true);
	IRenderTarget* GetRenderTarget() const;

	// Методы для управления depth buffer
	void SetDepthBuffer(DepthBuffer* depthBuffer);
	DepthBuffer* GetDepthBuffer() const;

	void Clear(const float4& color);	 // очистка render target
	void ClearDepth(float depth = 1.0f); // очистка depth buffer

	// Режимы отсечения и заполнения
	void SetCullMode(CullMode mode);
	CullMode GetCullMode() const;

	void SetFillMode(FillMode mode);
	FillMode GetFillMode() const;

	// Вьюпорт
	void SetViewport(const Viewport& vp);
	Viewport GetViewport() const;

	// Тайловый рендеринг
	void SetTileRenderingState(bool enable);
	bool GetTileRenderingState() const;

	void SetTileSize(uint32_t size);
	uint32_t GetTileSize() const;

	// Проверка корректности текущего состояния
	// Возвращает true, если состояние готово к рисованию
	// Если передан указатель на строку, в неё будет записано описание ошибки (при false)
	bool Validate(std::string* errorMsg = nullptr) const;

	void DrawIndexed(uint32_t indexCount, uint32_t startIndex);
	void DrawIndexed();
	void DrawFullScreenQuad();

  private:
	VertexShader m_VertexShader;
	GeometryShader m_GeometryShader;
	PixelShader m_PixelShader;

	VertexBuffer m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	ConstantBuffer m_ConstantBuffer;

	std::unique_ptr<DepthBuffer> m_OwnDepthBuffer;
	DepthBuffer* m_DepthBuffer;
	IRenderTarget* m_RenderTarget;

	CullMode m_cullMode;
	FillMode m_fillMode;

	Viewport m_Viewport;

	bool m_EnableTiledRendering;
	uint32_t m_TileSize;

	// Данные для тайлового рендера
    std::vector<Tile> m_tiles;
    std::vector<VertexOutput> m_transformedVerts;
    std::vector<int3> m_triangles;

    // Внутренние методы тайлового рендера
    void buildTiles(int width, int height);
    void binTriangles(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles);
    void renderTilesMultithreaded();
    void renderTilesSingleThreaded();
	void renderTile(int tileIndex);
	void renderTileQuad(int tileIndex, float invW, float invH);

    // Методы растеризации
	void DrawPoint(int x, int y, float z, const float4& color);
    void DrawLine(int x0, int y0, int x1, int y1, float z0, float z1, const float4& color);
    void RasterizeTriangle(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2);
    void RasterizeTriangleSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2);
    void RasterizeTriangleTile(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax);
    void RasterizeTriangleTileSSE(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, int2 tileMin, int2 tileMax);

	float4 ClipToScreen(const float4& clipPos) const;
	VertexOutput trilerp(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float a, float b, float c);

	inline float edgeFunction(const float4& a, const float4& b, const float2& c)
	{
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
	}
	inline float edgeFunction(const float4& a, const float4& b, const float4& c)
	{
		return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
	}
};

SOFTX_END
