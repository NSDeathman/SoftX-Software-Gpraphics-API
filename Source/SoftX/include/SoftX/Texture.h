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

	void saveToTGA(const TextureRGBA32F& tex, const char* filename)
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
