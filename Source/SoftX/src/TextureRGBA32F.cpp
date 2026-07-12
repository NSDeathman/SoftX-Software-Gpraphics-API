/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include "ThreadPoolManager.h"
#include "../include/SoftX.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

void TextureRGBA32F::GenerateMips() {
    mipChain.resize(1);
    uint w = resolution.x;
    uint h = resolution.y;

    while (w > 1 || h > 1) {
        uint nw = std::max(1u, w / 2);
        uint nh = std::max(1u, h / 2);
        std::vector<__m128> level(nw * nh);
        const auto& prev = mipChain.back();

        for (uint y = 0; y < nh; ++y) {
            for (uint x = 0; x < nw; ++x) {
                uint base = 2 * y * w + 2 * x;
                __m128 a = prev[base];
                __m128 b = prev[base + 1];
                __m128 c = prev[base + w];
                __m128 d = prev[base + w + 1];

                __m128 sum = _mm_add_ps(
                    _mm_add_ps(a, b),
                    _mm_add_ps(c, d));
                level[y * nw + x] = _mm_mul_ps(sum, _mm_set1_ps(0.25f));
            }
        }
        mipChain.push_back(std::move(level));
        w = nw;
        h = nh;
    }
}

void TextureRGBA32F::SaveToTGA(const char* filename) const {
    uint w = Width();
    uint h = Height();

    uint8_t header[18] = { 0 };
    header[2] = 2;
    header[12] = w & 0xFF;
    header[13] = (w >> 8) & 0xFF;
    header[14] = h & 0xFF;
    header[15] = (h >> 8) & 0xFF;
    header[16] = 32;
    header[17] = 8 | (1 << 5);

    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open file for writing: " << filename << std::endl;
        return;
    }
    file.write(reinterpret_cast<const char*>(header), 18);

    for (uint y = 0; y < h; ++y) {
        for (uint x = 0; x < w; ++x) {
            __m128 color = Read(uint2(x, y));
            float rgba[4];
            _mm_storeu_ps(rgba, color);
            uint8_t b = static_cast<uint8_t>(AfterMath::clamp(rgba[2] * 255.0f, 0.0f, 255.0f));
            uint8_t g = static_cast<uint8_t>(AfterMath::clamp(rgba[1] * 255.0f, 0.0f, 255.0f));
            uint8_t r = static_cast<uint8_t>(AfterMath::clamp(rgba[0] * 255.0f, 0.0f, 255.0f));
            uint8_t a = static_cast<uint8_t>(AfterMath::clamp(rgba[3] * 255.0f, 0.0f, 255.0f));
            uint8_t pixel[4] = { b, g, r, a };
            file.write(reinterpret_cast<const char*>(pixel), 4);
        }
    }
    file.close();
    std::cout << "Texture saved to " << filename << std::endl;
}

SOFTX_END
/////////////////////////////////////////////////////////////////
