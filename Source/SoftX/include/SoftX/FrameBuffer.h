#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include <iostream>

#include "ThirdPartyIncluding.h"
#include "RenderTargetInterface.h"
#include "ThreadPoolManager.h"
#include <xmmintrin.h>

SOFTX_BEGIN

class SOFTX_API Framebuffer : public IRenderTarget
{
public:
    Framebuffer(int2 size)
        : m_width(size.x), m_height(size.y), m_pixels(size.x * size.y)
    {
        clear(float4(0,0,0,1));
    }

    void clear(const float4& color) override
    {
        __m128 col = color.get_simd();
        __m128* data = m_pixels.data();
        size_t count = m_pixels.size();
        size_t i = 0;
        // Заполняем блоками по 4 пикселя (16 байт) потоковой записью
        for (; i + 4 <= count; i += 4)
        {
            _mm_stream_ps(reinterpret_cast<float*>(data + i), col);
            _mm_stream_ps(reinterpret_cast<float*>(data + i + 1), col);
            _mm_stream_ps(reinterpret_cast<float*>(data + i + 2), col);
            _mm_stream_ps(reinterpret_cast<float*>(data + i + 3), col);
        }
        _mm_sfence(); // гарантировать видимость
        for (; i < count; ++i)
        {
            data[i] = col;
        }
    }

    void set_pixel(int2 coords, const float4& color) override
    {
        if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
        {
            __m128* data = m_pixels.data();
            int index = coords.y * m_width + coords.x;
			_mm_stream_ps(reinterpret_cast<float*>(&data[index]), color.get_simd());
        }
    }

    void set_pixel(int2 coords, __m128 color)
    {
        if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
        {
            __m128* data = m_pixels.data();
            int index = coords.y * m_width + coords.x;
            _mm_stream_ps(reinterpret_cast<float*>(&data[index]), color);
        }
    }

    __m128 read(int2 coords) const
    {
        if (coords.x >= 0 && coords.x < m_width && coords.y >= 0 && coords.y < m_height)
        {
            return m_pixels[coords.y * m_width + coords.x];
        }
        return _mm_setzero_ps();
    }

    int width() const override { return m_width; }
    int height() const override { return m_height; }
    int2 size() const override { return int2(m_width, m_height); }

    void convertTile(int tileIdx, int tileSize, uint32_t* bgraBuffer, int width, int height) const
	{
		int tilesX = (width + tileSize - 1) / tileSize;
		int tx = tileIdx % tilesX;
		int ty = tileIdx / tilesX;
		int x0 = tx * tileSize;
		int y0 = ty * tileSize;
		int x1 = std::min(x0 + tileSize, width);
		int y1 = std::min(y0 + tileSize, height);

		__m128 scale = _mm_set1_ps(255.0f);

		for (int y = y0; y < y1; ++y)
		{
			const __m128* srcRow = m_pixels.data() + y * width;
			uint32_t* dstRow = bgraBuffer + y * width;
			for (int x = x0; x < x1; ++x)
			{
				// Загружаем один пиксель
				__m128 c = srcRow[x];
				// Преобразуем
				__m128i i = _mm_cvtps_epi32(_mm_mul_ps(c, scale));
				// Упаковываем в 8 бит (один пиксель – 4 байта)
				// Используем SSE для четырёх пикселей, но здесь один, поэтому проще скалярно
				float rgba[4];
				_mm_storeu_ps(rgba, c);
				uint8_t r = (uint8_t)(std::clamp(rgba[0], 0.0f, 1.0f) * 255.0f);
				uint8_t g = (uint8_t)(std::clamp(rgba[1], 0.0f, 1.0f) * 255.0f);
				uint8_t b = (uint8_t)(std::clamp(rgba[2], 0.0f, 1.0f) * 255.0f);
				uint8_t a = (uint8_t)(std::clamp(rgba[3], 0.0f, 1.0f) * 255.0f);
				dstRow[x] = (a << 24) | (b << 16) | (g << 8) | r; // BGRA
			}
		}
	}

	void present(HDC hdc, int2 dstPos, int2 dstSize) const
	{
		PROFILE_SCOPE("Present to GDI")

		int dstW = (dstSize.x == -1) ? m_width : dstSize.x;
		int dstH = (dstSize.y == -1) ? m_height : dstSize.y;

        alignas(16) std::vector<uint32_t> bgraBuffer(m_width * m_height);

        {
			PROFILE_SCOPE("Converting tiles")

			const int tileSize = 64;
			int tilesX = (m_width + tileSize - 1) / tileSize;
			int tilesY = (m_height + tileSize - 1) / tileSize;
			int numTiles = tilesX * tilesY;

			std::atomic<int> tileCounter(0);

			auto worker = [this, &tileCounter, numTiles, tileSize, &bgra = bgraBuffer, width = m_width,
						   height = m_height]() {
				while (true)
				{
					int idx = tileCounter.fetch_add(1);
					if (idx >= numTiles)
						break;
					convertTile(idx, tileSize, bgra.data(), width, height);
				}
			};

            auto& pool = ThreadPoolManager::Get();
			int numThreads = (int)pool.threadCount();
			for (int i = 0; i < numThreads; ++i)
			{
				pool.enqueue(worker);
			}
			pool.wait();
		}

        {
			PROFILE_SCOPE("SetDIBitsToDevice");

			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = m_width;
			bmi.bmiHeader.biHeight = -m_height;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;
			bmi.bmiHeader.biSizeImage = 0;

            SetDIBitsToDevice(hdc, dstPos.x, dstPos.y, dstW, dstH, 0, 0, 0, m_height, bgraBuffer.data(), &bmi, DIB_RGB_COLORS);
		}
	}

    bool saveTGA(const char* filename) const
    {
        std::ofstream file(filename, std::ios::binary);
        if (!file) return false;

        uint8_t header[18] = {0};
        header[2] = 2;
        header[12] = m_width & 0xFF;
        header[13] = (m_width >> 8) & 0xFF;
        header[14] = m_height & 0xFF;
        header[15] = (m_height >> 8) & 0xFF;
        header[16] = 32;
        header[17] = 8 | (1 << 5);
        file.write(reinterpret_cast<const char*>(header), 18);

        // Converting to BGRA
        std::vector<uint32_t> bgraPixels(m_width * m_height);
        const __m128* src = m_pixels.data();
        uint32_t* dst = bgraPixels.data();

        for (size_t i = 0; i < m_pixels.size(); ++i)
        {
            float rgba[4];
            _mm_storeu_ps(rgba, src[i]);
            uint8_t r = (uint8_t)(std::clamp(rgba[0], 0.0f, 1.0f) * 255.0f);
            uint8_t g = (uint8_t)(std::clamp(rgba[1], 0.0f, 1.0f) * 255.0f);
            uint8_t b = (uint8_t)(std::clamp(rgba[2], 0.0f, 1.0f) * 255.0f);
            uint8_t a = (uint8_t)(std::clamp(rgba[3], 0.0f, 1.0f) * 255.0f);
            dst[i] = (a << 24) | (b << 16) | (g << 8) | r; // TGA order - BGRA (0xAABBGGRR in little-endian)
        }

        file.write(reinterpret_cast<const char*>(dst), m_pixels.size() * 4);
        file.close();
        return true;
    }

private:
    int m_width, m_height;
    std::vector<__m128> m_pixels;
};

SOFTX_END
