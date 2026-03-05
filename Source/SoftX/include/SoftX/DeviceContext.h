#pragma once

#include "ThirdPartyIncluding.h"
#include "LibInternal.h"
#include "Types.h"
#include "RenderTargetInterface.h"
#include "RasterizerInterface.h"

SOFTX_BEGIN

class SOFTX_API DeviceContext
{
  public:
	DeviceContext();
	~DeviceContext();

	DeviceContext(DeviceContext&&) = default;
	DeviceContext& operator=(DeviceContext&&) = default;

	void SetVertexShader(VertexShader shader);
	VertexShader GetVertexShader() const;

	void SetGeometryShader(GeometryShader shader);
	GeometryShader GetGeometryShader() const;

	void SetPixelShader(PixelShader shader);
	PixelShader GetPixelShader() const;

	void SetVertexBuffer(const VertexBuffer& buffer);
	VertexBuffer GetVertexBuffer() const;

	void SetIndexBuffer(const IndexBuffer& buffer);
	IndexBuffer GetIndexBuffer() const;

	void SetConstantBuffer(const ConstantBuffer& buffer);
	ConstantBuffer GetConstantBuffer() const;

	void SetRenderTarget(IRenderTarget* target);
	void SetRenderTarget(IRenderTarget* target, bool createDepthBuffer = true);
	IRenderTarget* GetRenderTarget() const;

	// Методы для управления depth buffer
	void SetDepthBuffer(DepthBuffer* depthBuffer);
	DepthBuffer* GetDepthBuffer() const;

	void Clear(const float4& color);
	void ClearDepth(float depth = 1.0f);

	void SetCullMode(CullMode mode);
	CullMode GetCullMode() const;

	void SetFillMode(FillMode mode);
	FillMode GetFillMode() const;

	void SetViewport(const Viewport& vp);
	Viewport GetViewport() const;

	void SetTileRenderingState(bool enable);
	bool GetTileRenderingState() const;

	void SetTileSize(uint32_t size);
	uint32_t GetTileSize() const;

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

	std::unique_ptr<IRasterizer> m_Rasterizer;

	CullMode m_cullMode;
	FillMode m_fillMode;

	Viewport m_Viewport;

	bool m_EnableTiledRendering;
	uint32_t m_TileSize;

    std::vector<Tile> m_tiles;
    std::vector<VertexOutput> m_transformedVerts;
    std::vector<int3> m_triangles;

    void buildTiles(int width, int height);
    void binTriangles(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles);
    void renderTiles();
	void renderTile(int tileIndex);
	void renderTileQuad(int tileIndex, float invW, float invH);

	void DrawPoint(int x, int y, float z, const float4& color);
    void DrawLine(int x0, int y0, int x1, int y1, float z0, float z1, const float4& color);

	float4 ClipToScreen(const float4& clipPos) const;
};

SOFTX_END
