#pragma once
#include "RenderTargetInterface.h"
#include "Texture.h"

class RenderTargetTexture : public IRenderTarget
{
  public:
	RenderTargetTexture(int2 size) : m_texture(size)
	{
	}

	void clear(const float4& color) override
	{
		// «аполн€ем текстуру цветом
		int w = m_texture.width();
		int h = m_texture.height();
		__m128 col = _mm_set_ps(color.w, color.z, color.y, color.x); // float4 RGBA -> __m128 (w,z,y,x)
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				m_texture.stream_write(int2(x, y), col); // можно использовать обычную запись
			}
		}
	}

	void set_pixel(int2 coords, const float4& color) override
	{
		__m128 col = _mm_set_ps(color.w, color.z, color.y, color.x);
		m_texture.stream_write(coords, col);
	}

	int width() const override
	{
		return m_texture.width();
	}
	int height() const override
	{
		return m_texture.height();
	}
	int2 size() const override
	{
		return int2(width(), height());
	}

	// ƒоступ к текстуре дл€ использовани€ в шейдерах
	const TextureRGBA32F& texture() const
	{
		return m_texture;
	}
	TextureRGBA32F& texture()
	{
		return m_texture;
	}

  private:
	TextureRGBA32F m_texture;
};
