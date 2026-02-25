#pragma once

#include <xmmintrin.h>   // SSE
#include <vector>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include "MicroMath.h"

class TextureRGBA32F {
public:
    TextureRGBA32F(int2 size)
        : m_width(size.x), m_height(size.y), m_pixels(size.x* size.y) {
        // Инициализация нулями
        __m128 zero = _mm_setzero_ps();
        for (auto& p : m_pixels) {
            _mm_store_ps(reinterpret_cast<float*>(&p), zero);
        }
    }

    // Чтение пикселя (const, можно использовать параллельно)
    __m128 read(int2 coords) const {
        assert(coords.x >= 0 && coords.x < m_width&& coords.y >= 0 && coords.y < m_height);
        return m_pixels[coords.y * m_width + coords.x];
    }

    // Чтение по индексу (для сэмплирования)
    __m128 read(int index) const {
        assert(index >= 0 && index < (int)m_pixels.size());
        return m_pixels[index];
    }

    // Потоковая запись одного пикселя (некэшируемая)
    void stream_write(int2 coords, __m128 color) {
        assert(coords.x >= 0 && coords.x < m_width&& coords.y >= 0 && coords.y < m_height);
        int index = coords.y * m_width + coords.x;
        // Адрес должен быть выровнен по 16 байт – это гарантируется std::vector<__m128> начиная с C++17
        _mm_stream_ps(reinterpret_cast<float*>(&m_pixels[index]), color);
    }

    // Потоковая запись по индексу (удобно для тайлов)
    void stream_write(int index, __m128 color) {
        assert(index >= 0 && index < (int)m_pixels.size());
        _mm_stream_ps(reinterpret_cast<float*>(&m_pixels[index]), color);
    }

    // Получить размеры
    int width() const { return m_width; }
    int height() const { return m_height; }

private:
    int m_width, m_height;
    std::vector<__m128> m_pixels;
};

void saveTextureToTGA(const TextureRGBA32F& tex, const char* filename) {
    int w = tex.width();
    int h = tex.height();

    // Заголовок TGA (18 байт)
    uint8_t header[18] = { 0 };
    header[2] = 2;                         // Несжатый true-color
    header[12] = w & 0xFF;                  // ширина (младший байт)
    header[13] = (w >> 8) & 0xFF;           // ширина (старший байт)
    header[14] = h & 0xFF;                  // высота (младший байт)
    header[15] = (h >> 8) & 0xFF;           // высота (старший байт)
    header[16] = 32;                        // бит на пиксель (RGBA)
    header[17] = 8 | (1 << 5);               // 8 бит альфа, origin top-left (бит 5 установлен)

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        return;
    }
    file.write(reinterpret_cast<char*>(header), 18);

    // Запись пикселей в порядке BGR (TGA порядок)
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            __m128 color = tex.read(int2(x, y)); // читаем float4
            float rgba[4];
            _mm_storeu_ps(rgba, color); // сохраняем в массив (можно невыровненно)

            // Конвертируем из float [0,1] в байты и меняем порядок на BGRA
            uint8_t b = static_cast<uint8_t>(std::clamp(rgba[2] * 255.0f, 0.0f, 255.0f)); // blue
            uint8_t g = static_cast<uint8_t>(std::clamp(rgba[1] * 255.0f, 0.0f, 255.0f)); // green
            uint8_t r = static_cast<uint8_t>(std::clamp(rgba[0] * 255.0f, 0.0f, 255.0f)); // red
            uint8_t a = static_cast<uint8_t>(std::clamp(rgba[3] * 255.0f, 0.0f, 255.0f)); // alpha

            uint8_t pixel[4] = { b, g, r, a }; // TGA порядок: BGRA
            file.write(reinterpret_cast<char*>(pixel), 4);
        }
    }
    file.close();
    std::cout << "Texture saved to " << filename << std::endl;
}
