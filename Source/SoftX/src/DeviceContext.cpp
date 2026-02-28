#include "pch.h"

#include <SoftX/SoftX.h>

SOFTX_BEGIN

DeviceContext::DeviceContext() : 
	m_VertexShader(nullptr), 
	m_PixelShader(nullptr), 
	m_VertexBuffer(), 
	m_IndexBuffer(), 
	m_ConstantBuffer(),
	m_RenderTarget(nullptr), 
	m_cullMode(CullMode::Back), 
	m_fillMode(FillMode::Solid), 
	m_Viewport(),
	m_EnableTiledRendering(true), 
	m_TileSize(64)
{
}

DeviceContext::~DeviceContext() = default;

void DeviceContext::SetVertexShader(VertexShader shader)
{
	m_VertexShader = std::move(shader);
}

VertexShader DeviceContext::GetVertexShader() const
{
	return m_VertexShader;
}

void DeviceContext::SetPixelShader(PixelShader shader)
{
	m_PixelShader = std::move(shader);
}

PixelShader DeviceContext::GetPixelShader() const
{
	return m_PixelShader;
}

void DeviceContext::SetVertexBuffer(const VertexBuffer& buffer)
{
	m_VertexBuffer = buffer;
}

VertexBuffer DeviceContext::GetVertexBuffer() const
{
	return m_VertexBuffer;
}

void DeviceContext::SetIndexBuffer(const IndexBuffer& buffer)
{
	m_IndexBuffer = buffer;
}

IndexBuffer DeviceContext::GetIndexBuffer() const
{
	return m_IndexBuffer;
}

void DeviceContext::SetConstantBuffer(const ConstantBuffer& buffer)
{
	m_ConstantBuffer = buffer;
}

ConstantBuffer DeviceContext::GetConstantBuffer() const
{
	return m_ConstantBuffer;
}

void DeviceContext::SetRenderTarget(IRenderTarget* target)
{
	m_RenderTarget = target;
}

IRenderTarget* DeviceContext::GetRenderTarget() const
{
	return m_RenderTarget;
}

void DeviceContext::SetCullMode(CullMode mode)
{
	m_cullMode = mode;
}

CullMode DeviceContext::GetCullMode() const
{
	return m_cullMode;
}

void DeviceContext::SetFillMode(FillMode mode)
{
	m_fillMode = mode;
}

FillMode DeviceContext::GetFillMode() const
{
	return m_fillMode;
}

void DeviceContext::SetViewport(const Viewport& vp)
{
	m_Viewport = vp;
}

Viewport DeviceContext::GetViewport() const
{
	return m_Viewport;
}

void DeviceContext::SetTileRenderingState(bool enable)
{
	m_EnableTiledRendering = enable;
}

bool DeviceContext::GetTileRenderingState() const
{
	return m_EnableTiledRendering;
}

void DeviceContext::SetTileSize(uint32_t size)
{
	m_TileSize = size;
}

uint32_t DeviceContext::GetTileSize() const
{
	return m_TileSize;
}

bool DeviceContext::Validate(std::string* errorMsg) const
{
	bool bCheckResult = true;

	// Проверка вершинного шейдера
	if (!m_VertexShader)
	{
		if (errorMsg)
			*errorMsg = "Vertex shader not set ";
		bCheckResult = false;
	}
	// Проверка пиксельного шейдера
	if (!m_PixelShader)
	{
		if (errorMsg)
			*errorMsg += "Pixel shader not set ";
		bCheckResult = false;
	}
	// Проверка вершинного буфера
	if (m_VertexBuffer.IsEmpty())
	{
		if (errorMsg)
			*errorMsg += "Vertex buffer is empty ";
		bCheckResult = false;
	}
	// Проверка индексного буфера
	if (m_IndexBuffer.IsEmpty())
	{
		if (errorMsg)
			*errorMsg += "Index buffer is empty ";
		bCheckResult = false;
	}
	// Проверка рендертаргета
	if (m_RenderTarget == nullptr)
	{
		if (errorMsg)
			*errorMsg += "Render target not set ";
		bCheckResult = false;
	}
	// Проверка viewport (размеры должны быть положительными)
	if (m_Viewport.size.x <= 0.0f || m_Viewport.size.y <= 0.0f)
	{
		if (errorMsg)
			*errorMsg += "Viewport has non-positive size ";
		bCheckResult = false;
	}
	// Проверка размера тайла (для тайлового рендеринга)
	if (m_TileSize == 0)
	{
		if (errorMsg)
			*errorMsg += "Tile size is zero ";
		bCheckResult = false;
	}

	return bCheckResult;
}

SOFTX_END
