#pragma once

#include <xmmintrin.h>
#include <vector>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>

#include "ThirdPartyIncluding.h"
#include "LibInternal.h"

SOFTX_BEGIN

class SOFTX_API TextureRGBA32F
{
  public:
	TextureRGBA32F(int2 size) : m_width(size.x), m_height(size.y), m_pixels(size.x * size.y)
	{
		__m128 zero = _mm_setzero_ps();
		for (auto& p : m_pixels)
		{
			_mm_store_ps(reinterpret_cast<float*>(&p), zero);
		}
	}

	__m128 read(int2 coords) const
	{
		assert(coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height);
		return m_pixels[coords.y * m_width + coords.x];
	}

	__m128 read(int index) const
	{
		assert(index >= 0 && index < (int)m_pixels.size());
		return m_pixels[index];
	}

	__m128 sample_raw(float2 uv) const
	{
		int x = (int)(uv.x * m_width);
		int y = (int)(uv.y * m_height);
		if (x < 0)
			x = 0;
		if (x >= m_width)
			x = m_width - 1;
		if (y < 0)
			y = 0;
		if (y >= m_height)
			y = m_height - 1;
		return m_pixels[y * m_width + x];
	}

	float4 sample(float2 uv) const
	{
		__m128 color = sample_raw(uv);
		return float4(color);
	}

	__m128 fetch_raw(int x, int y) const
	{
		x = std::clamp(x, 0, m_width - 1);
		y = std::clamp(y, 0, m_height - 1);
		return m_pixels[y * m_width + x];
	}

	__m128 sample_bilinear_raw(float2 uv) const
	{
		float fx = uv.x * m_width - 0.5f;
		float fy = uv.y * m_height - 0.5f;

		int x0 = (int)std::floor(fx);
		int y0 = (int)std::floor(fy);

		float tx = fx - x0;
		float ty = fy - y0;

		// Загружаем 4 угловых пикселя — каждый уже __m128 (RGBA float)
		__m128 c00 = fetch_raw(x0, y0);
		__m128 c10 = fetch_raw(x0 + 1, y0);
		__m128 c01 = fetch_raw(x0, y0 + 1);
		__m128 c11 = fetch_raw(x0 + 1, y0 + 1);

		// Веса как __m128 — broadcast скаляра на все 4 канала
		__m128 wtx = _mm_set1_ps(tx);
		__m128 wty = _mm_set1_ps(ty);
		__m128 one = _mm_set1_ps(1.0f);
		__m128 w1tx = _mm_sub_ps(one, wtx); // (1 - tx)
		__m128 w1ty = _mm_sub_ps(one, wty); // (1 - ty)

		// Билинейная интерполяция:
		// result = c00*(1-tx)*(1-ty) + c10*tx*(1-ty) + c01*(1-tx)*ty + c11*tx*ty
		__m128 w00 = _mm_mul_ps(w1tx, w1ty);
		__m128 w10 = _mm_mul_ps(wtx, w1ty);
		__m128 w01 = _mm_mul_ps(w1tx, wty);
		__m128 w11 = _mm_mul_ps(wtx, wty);

		__m128 result = _mm_add_ps(_mm_add_ps(_mm_mul_ps(c00, w00), _mm_mul_ps(c10, w10)),
								   _mm_add_ps(_mm_mul_ps(c01, w01), _mm_mul_ps(c11, w11)));
		return result;
	}

	// Публичный float4 вариант — делегирует в raw версию
	float4 sample_bilinear(float2 uv) const
	{
		return float4(sample_bilinear_raw(uv));
	}

	void stream_write(int2 coords, __m128 color)
	{
		assert(coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height);
		int index = coords.y * m_width + coords.x;
		// Adress must be aligned as 16 bits – it's present by default in std::vector<__m128> begins from C++17
		_mm_stream_ps(reinterpret_cast<float*>(&m_pixels[index]), color);
	}

	void stream_write(int index, __m128 color)
	{
		assert(index >= 0 && index < (int)m_pixels.size());
		_mm_stream_ps(reinterpret_cast<float*>(&m_pixels[index]), color);
	}

	int width() const
	{
		return m_width;
	}
	int height() const
	{
		return m_height;
	}

	void saveToTGA(const TextureRGBA32F& tex, const char* filename) const
	{
		int w = tex.width();
		int h = tex.height();

		// TGA header (18 bytes)
		uint8_t header[18] = { 0 };
		header[2] = 2;				  // Uncompressed true-color
		header[12] = w & 0xFF;		  // width
		header[13] = (w >> 8) & 0xFF; // width
		header[14] = h & 0xFF;		  // height
		header[15] = (h >> 8) & 0xFF; // height
		header[16] = 32;			  // bits for pixel (RGBA)
		header[17] = 8 | (1 << 5);	  // 8 bits, origin top-left

		std::ofstream file(filename, std::ios::binary);
		if (!file)
		{
			std::cerr << "Cannot open file for writing: " << filename << std::endl;
			return;
		}
		file.write(reinterpret_cast<char*>(header), 18);

		// Pixels writing in BGR (TGA order)
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				__m128 color = tex.read(int2(x, y));
				float rgba[4];
				_mm_storeu_ps(rgba, color);

				uint8_t b = static_cast<uint8_t>(std::clamp(rgba[2] * 255.0f, 0.0f, 255.0f)); // blue
				uint8_t g = static_cast<uint8_t>(std::clamp(rgba[1] * 255.0f, 0.0f, 255.0f)); // green
				uint8_t r = static_cast<uint8_t>(std::clamp(rgba[0] * 255.0f, 0.0f, 255.0f)); // red
				uint8_t a = static_cast<uint8_t>(std::clamp(rgba[3] * 255.0f, 0.0f, 255.0f)); // alpha

				uint8_t pixel[4] = { b, g, r, a }; // TGA order: BGRA
				file.write(reinterpret_cast<char*>(pixel), 4);
			}
		}
		file.close();
		std::cout << "Texture saved to " << filename << std::endl;
	}

  private:
	int m_width, m_height;
	std::vector<__m128> m_pixels;
};

SOFTX_END
