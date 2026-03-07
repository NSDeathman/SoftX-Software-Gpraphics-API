#pragma once

#include "ThirdPartyIncluding.h"
#include "RenderTargetInterface.h"
#include "Texture.h"
#include "LibInternal.h"

SOFTX_BEGIN

class SOFTX_API RenderTargetTexture : public IRenderTarget
{
  public:
	RenderTargetTexture(uint2 size) : texture(size)
	{
	}

	void Clear(const float4& color) override
	{
		int w = texture.Width();
		int h = texture.Height();
		__m128 col = _mm_set_ps(color.w, color.z, color.y, color.x); // float4 RGBA -> __m128 (w,z,y,x)
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
                texture.StreamWrite(int2(x, y), col);
			}
		}
	}

	void SetPixel(int2 coords, const float4& color) override
	{
		__m128 col = _mm_set_ps(color.w, color.z, color.y, color.x);
		texture.StreamWrite(coords, col);
	}

	uint Width() const override
	{
		return texture.Width();
	}
	uint Height() const override
	{
		return texture.Height();
	}
	uint2 Size() const override
	{
		return uint2(Width(), Height());
	}

	const TextureRGBA32F& Texture() const
	{
		return texture;
	}
	TextureRGBA32F& Texture()
	{
		return texture;
	}

  private:
	TextureRGBA32F texture;
};

SOFTX_END
