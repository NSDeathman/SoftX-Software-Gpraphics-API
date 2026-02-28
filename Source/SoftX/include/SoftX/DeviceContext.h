#pragma once

#include "LibInternal.h"
#include "Types.h"
#include "RenderTargetInterface.h"

SOFTX_BEGIN

class SOFTX_API DeviceContext
{
  public:
	DeviceContext();
	~DeviceContext();

	// Сеттеры и геттеры для шейдеров
	void SetVertexShader(VertexShader shader);
	VertexShader GetVertexShader() const;

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
	IRenderTarget* GetRenderTarget() const;

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

  private:
	VertexShader m_VertexShader;
	PixelShader m_PixelShader;

	VertexBuffer m_VertexBuffer;
	IndexBuffer m_IndexBuffer;
	ConstantBuffer m_ConstantBuffer;

	IRenderTarget* m_RenderTarget;

	CullMode m_cullMode;
	FillMode m_fillMode;

	Viewport m_Viewport;

	bool m_EnableTiledRendering;
	uint32_t m_TileSize;
};

SOFTX_END
