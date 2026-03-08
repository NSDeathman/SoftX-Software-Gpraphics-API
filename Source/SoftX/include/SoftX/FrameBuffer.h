#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
#include <windows.h>
#include <xmmintrin.h>

#include "RenderTargetInterface.h"
#include "ThirdPartyIncluding.h"
#include "ThreadPoolManager.h"

SOFTX_BEGIN

class SOFTX_API Framebuffer : public IRenderTarget
{
public:
    explicit Framebuffer(uint2 size) : resolution(size), pixelsStorage(size.x * size.y)
    {
        Clear(float4(0, 0, 0, 1));
    }

    void Clear(const float4& color) override
    {
        __m128 col = color.get_simd();
        __m128* data = pixelsStorage.data();
        size_t count = pixelsStorage.size();
        size_t i = 0;

        // Fill with blocks of 4 pixels (16 bytes) using streaming writes
        for (; i + 4 <= count; i += 4)
        {
            _mm_stream_ps(reinterpret_cast<float*>(data + i), col);
            _mm_stream_ps(reinterpret_cast<float*>(data + i + 1), col);
            _mm_stream_ps(reinterpret_cast<float*>(data + i + 2), col);
            _mm_stream_ps(reinterpret_cast<float*>(data + i + 3), col);
        }
        _mm_sfence(); // Ensure visibility

        for (; i < count; ++i)
        {
            data[i] = col;
        }
    }

    void SetPixel(uint2 coords, const float4& color) override
    {
        __m128* data = pixelsStorage.data();
        uint index = coords.y * resolution.x + coords.x;
        _mm_stream_ps(reinterpret_cast<float*>(&data[index]), color.get_simd());
    }

    void SetPixel(uint2 coords, __m128 color)
    {
        __m128* data = pixelsStorage.data();
        uint index = coords.y * resolution.x + coords.x;
        _mm_stream_ps(reinterpret_cast<float*>(&data[index]), color);
    }

    __m128 Read(uint2 coords) const
    {
        return pixelsStorage[coords.y * resolution.x + coords.x];
    }

    uint Width() const override
    {
        return resolution.x;
    }

    uint Height() const override
    {
        return resolution.y;
    }

    uint2 Size() const override
    {
        return resolution;
    }

    void ConvertTile(uint tileIdx, uint tileSize, uint* bgraBuffer) const
    {
        int tilesX = (resolution.x + tileSize - 1) / tileSize;
        int tx = tileIdx % tilesX;
        int ty = tileIdx / tilesX;
        int x0 = tx * tileSize;
        int y0 = ty * tileSize;
        int x1 = std::min(x0 + (int)tileSize, (int)resolution.x);
        int y1 = std::min(y0 + (int)tileSize, (int)resolution.y);

        __m128 scale = _mm_set1_ps(255.0f);

        for (int y = y0; y < y1; ++y)
        {
            const __m128* srcRow = pixelsStorage.data() + y * resolution.x;
            uint* dstRow = bgraBuffer + y * resolution.x;
            for (int x = x0; x < x1; ++x)
            {
                // Load one pixel
                __m128 c = srcRow[x];

                // Convert to BGRA using scalar approach
                float rgba[4];
                _mm_storeu_ps(rgba, c);
                uint8_t r = (uint8_t)(clamp(rgba[0], 0.0f, 1.0f) * 255.0f);
                uint8_t g = (uint8_t)(clamp(rgba[1], 0.0f, 1.0f) * 255.0f);
                uint8_t b = (uint8_t)(clamp(rgba[2], 0.0f, 1.0f) * 255.0f);
                uint8_t a = (uint8_t)(clamp(rgba[3], 0.0f, 1.0f) * 255.0f);
                dstRow[x] = (a << 24) | (b << 16) | (g << 8) | r; // BGRA
            }
        }
    }

    void Present(HDC hdc, int2 dstPos, int2 dstSize) const
    {
        PROFILE_SCOPE("Present framebuffer");

        int dstW = (dstSize.x == -1) ? (int)resolution.x : dstSize.x;
        int dstH = (dstSize.y == -1) ? (int)resolution.y : dstSize.y;

        alignas(16) std::vector<uint> bgraBuffer(resolution.x * resolution.y);

        {
            PROFILE_SCOPE("Converting tiles");

            const uint tileSize = 64;
            int tilesX = (resolution.x + tileSize - 1) / tileSize;
            int tilesY = (resolution.y + tileSize - 1) / tileSize;
            int numTiles = tilesX * tilesY;

            std::atomic<int> tileCounter(0);

            auto worker = [this, &tileCounter, numTiles, tileSize, &bgra = bgraBuffer]()
            {
                while (true)
                {
                    int idx = tileCounter.fetch_add(1);
                    if (idx >= numTiles)
                        break;
                    ConvertTile(idx, tileSize, bgra.data());
                }
            };

            auto& pool = ThreadPoolManager::Get();
            uint numThreads = (uint)pool.threadCount();
            for (uint i = 0; i < numThreads; ++i)
            {
                pool.enqueue(worker);
            }
            pool.wait();
        }

        {
            PROFILE_SCOPE("SetDIBitsToDevice");

            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = (LONG)resolution.x;
            bmi.bmiHeader.biHeight = -(LONG)resolution.y;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biSizeImage = 0;

            SetDIBitsToDevice(hdc, dstPos.x, dstPos.y, dstW, dstH, 0, 0, 0, resolution.y, bgraBuffer.data(), &bmi,
                              DIB_RGB_COLORS);
        }
    }

    bool SaveTGA(const char* filename) const
    {
        std::ofstream file(filename, std::ios::binary);
        if (!file)
            return false;

        uint8_t header[18] = {0};
        header[2] = 2;                           // Uncompressed true-color
        header[12] = resolution.x & 0xFF;        // width low byte
        header[13] = (resolution.x >> 8) & 0xFF; // width high byte
        header[14] = resolution.y & 0xFF;        // height low byte
        header[15] = (resolution.y >> 8) & 0xFF; // height high byte
        header[16] = 32;                         // bits per pixel (RGBA)
        header[17] = 8 | (1 << 5);               // 8 bits alpha, origin top-left
        file.write(reinterpret_cast<const char*>(header), 18);

        // Convert to BGRA
        std::vector<uint> bgraPixels(resolution.x * resolution.y);
        const __m128* src = pixelsStorage.data();
        uint* dst = bgraPixels.data();

        for (size_t i = 0; i < pixelsStorage.size(); ++i)
        {
            float rgba[4];
            _mm_storeu_ps(rgba, src[i]);
            uint8_t r = (uint8_t)(clamp(rgba[0], 0.0f, 1.0f) * 255.0f);
            uint8_t g = (uint8_t)(clamp(rgba[1], 0.0f, 1.0f) * 255.0f);
            uint8_t b = (uint8_t)(clamp(rgba[2], 0.0f, 1.0f) * 255.0f);
            uint8_t a = (uint8_t)(clamp(rgba[3], 0.0f, 1.0f) * 255.0f);
            dst[i] = (a << 24) | (b << 16) | (g << 8) | r; // BGRA in little-endian (0xAABBGGRR)
        }

        file.write(reinterpret_cast<const char*>(dst), pixelsStorage.size() * 4);
        file.close();
        return true;
    }

private:
    uint2 resolution;
    std::vector<__m128> pixelsStorage;
};

SOFTX_END
