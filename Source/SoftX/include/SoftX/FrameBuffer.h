#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <algorithm>

#include "Math.h"
#include "RenderTargetInterface.h"

SOFTX_BEGIN

class SOFTX_API Framebuffer : public IRenderTarget
{
  public:
	Framebuffer(int2 size)
	{
		m_width = size.x;
		m_height = size.y;
		m_pixels.resize(size.x * size.y);
		std::fill(m_pixels.begin(), m_pixels.end(), 0xFF000000);
	}

	// Очистка цветом float4 (компоненты в порядке RGBA)
	void clear(const float4& color) override
	{
		uint32_t c = float4ToBGRA(color);
		std::fill(m_pixels.begin(), m_pixels.end(), c);
	}

	// Очистка готовым 32-битным цветом (0xAARRGGBB)
	void clear(uint32_t color)
	{
		uint32_t* data = m_pixels.data();
		size_t count = m_pixels.size();
		__m128i color4 = _mm_set1_epi32(color);
		size_t i = 0;
		for (; i + 4 <= count; i += 4)
		{
			_mm_storeu_si128((__m128i*)(data + i), color4);
		}
		for (; i < count; ++i)
		{
			data[i] = color;
		}
	}

	// Установка пикселя по координатам (float4 цвет)
	void set_pixel(int2 coords, const float4& color) override
	{
		if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
		{
			m_pixels[coords.y * m_width + coords.x] = float4ToBGRA(color);
		}
	}

	// Установка пикселя готовым цветом (0xAARRGGBB)
	void set_pixel(int2 coords, uint32_t color)
	{
		if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
		{
			m_pixels[coords.y * m_width + coords.x] = color;
		}
	}

	// Чтение пикселя как 32-бит цвета (0xAARRGGBB)
	uint32_t get_pixel(int2 coords) const
	{
		if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
		{
			return m_pixels[coords.y * m_width + coords.x];
		}
		return 0;
	}

	// Получение указателя на данные для GDI
	const uint32_t* data() const
	{
		return m_pixels.data();
	}
	uint32_t* data()
	{
		return m_pixels.data();
	}

	// Размеры
	int width() const override
	{
		return m_width;
	}
	int height() const override
	{
		return m_height;
	}
	int2 size() const override
	{
		return int2(m_width, m_height);
	}

	// Сохранение в TGA (порядок BGRA уже совпадает с требуемым в файле)
	bool saveTGA(const char* filename) const
	{
		std::ofstream file(filename, std::ios::binary);
		if (!file)
		{
			std::cerr << "Cannot open file: " << filename << std::endl;
			return false;
		}

		uint8_t header[18] = {0};
		header[2] = 2; // uncompressed true-color
		header[12] = m_width & 0xFF;
		header[13] = (m_width >> 8) & 0xFF;
		header[14] = m_height & 0xFF;
		header[15] = (m_height >> 8) & 0xFF;
		header[16] = 32;		   // bits per pixel
		header[17] = 8 | (1 << 5); // 8 bits alpha, top-left origin
		file.write(reinterpret_cast<const char*>(header), 18);

		// Пиксели уже в порядке BGRA (как требует TGA)
		file.write(reinterpret_cast<const char*>(m_pixels.data()), m_pixels.size() * 4);
		file.close();
		return true;
	}

	// Вывод на GDI-контекст (например, в окне)
	void present(HDC hdc, int2 dstPos = int2(0, 0), int2 dstSize = int2(-1, -1)) const
	{
		int dstW = (dstSize.x == -1) ? m_width : dstSize.x;
		int dstH = (dstSize.y == -1) ? m_height : dstSize.y;

		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = m_width;
		bmi.bmiHeader.biHeight = -m_height; // top-down
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = 0;

		SetDIBitsToDevice(hdc, dstPos.x, dstPos.y, dstW, dstH, 0, 0, 0, m_height, m_pixels.data(), &bmi,
						  DIB_RGB_COLORS);
	}

  private:
	// Преобразование float4 (RGBA) в 32-бит BGRA (0xAARRGGBB)
	static uint32_t float4ToBGRA(const float4& c)
	{
		uint8_t r = static_cast<uint8_t>(std::clamp(c.x * 255.0f, 0.0f, 255.0f));
		uint8_t g = static_cast<uint8_t>(std::clamp(c.y * 255.0f, 0.0f, 255.0f));
		uint8_t b = static_cast<uint8_t>(std::clamp(c.z * 255.0f, 0.0f, 255.0f));
		uint8_t a = static_cast<uint8_t>(std::clamp(c.w * 255.0f, 0.0f, 255.0f));
		return (a << 24) | (r << 16) | (g << 8) | b; // 0xAARRGGBB
	}

	int m_width, m_height;
	std::vector<uint32_t> m_pixels;
};

SOFTX_END
