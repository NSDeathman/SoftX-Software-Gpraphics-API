/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>
#include <windows.h>
#include <xmmintrin.h>
#include <emmintrin.h>

#include "RenderTargetInterface.h"
#include "ThirdPartyIncluding.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class SOFTX_API Framebuffer : public IRenderTarget
{
public:
    explicit Framebuffer(uint2 size) : resolution(size), pixelsStorage(size.x * size.y, 0)
    {
    }

    uint32_t* GetRawPixels() { return pixelsStorage.data(); }
    const uint32_t* GetRawPixels() const { return pixelsStorage.data(); }

    static uint32_t PackColor(const float4& c)
    {
        auto toByte = [](float f) -> uint8_t {
            int v = int(f * 255.0f + 0.5f);
            return uint8_t(AfterMath::clamp(v, 0, 255));
        };
        uint8_t r = toByte(c.x);
        uint8_t g = toByte(c.y);
        uint8_t b = toByte(c.z);
        uint8_t a = toByte(c.w);
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    static __m128 UnpackColor(const uint32_t& bgra)
    {
        uint8_t r = (bgra >> 16) & 0xFF;
        uint8_t g = (bgra >> 8) & 0xFF;
        uint8_t b = (bgra >> 0) & 0xFF;
        uint8_t a = (bgra >> 24) & 0xFF;
        const float inv255 = 1.0f / 255.0f;
        return _mm_set_ps(a * inv255, b * inv255, g * inv255, r * inv255);
    }

    void Clear(const float4& color) override
    {
        uint32_t bg = PackColor(color);
        size_t count = pixelsStorage.size();
        size_t i = 0;

        __m128i bg4 = _mm_set1_epi32(bg);
        for (; i + 4 <= count; i += 4)
        {
            _mm_stream_si128(reinterpret_cast<__m128i*>(pixelsStorage.data() + i), bg4);
        }
        _mm_sfence();

        for (; i < count; ++i)
        {
            pixelsStorage[i] = bg;
        }
    }

    void SetPixel(const uint2& coords, const float4& color) override
    {
        uint index = coords.y * resolution.x + coords.x;
        pixelsStorage[index] = PackColor(color);
    }

    __m128 Read(const uint2& coords) const
    {
        uint32_t bg = pixelsStorage[coords.y * resolution.x + coords.x];
        return UnpackColor(bg);
    }

    uint Width() const override { return resolution.x; }
    uint Height() const override { return resolution.y; }
    uint2 Size() const override { return resolution; }

    void PresentBitmap(HDC hdc, const int2& dstPos, const int2& dstSize)
    {
        PROFILE_SCOPE("Framebuffer::PresentBitmap");

        int dstW = (dstSize.x == -1) ? (int)resolution.x : dstSize.x;
        int dstH = (dstSize.y == -1) ? (int)resolution.y : dstSize.y;

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = (LONG)resolution.x;
        bmi.bmiHeader.biHeight = -(LONG)resolution.y;   // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        SetDIBitsToDevice(hdc,
            dstPos.x, dstPos.y,
            dstW, dstH,
            0, 0,
            0,
            resolution.y,
            pixelsStorage.data(),
            &bmi,
            DIB_RGB_COLORS);
    }

    // Inspired by Onigiri :D
    // https://www.youtube.com/watch?v=n4zUgtDk95w
    // https://github.com/ArtemOnigiri/Console3D/blob/main/ConsoleRayTracing.cpp
    void PresentASCII(HANDLE hConsole, const uint2& consoleSize)
    {
        PROFILE_SCOPE("Framebuffer::PresentASCII");

        if (!hConsole || consoleSize.x == 0 || consoleSize.y == 0)
            return;

        uint srcW = resolution.x;
        uint srcH = resolution.y;
        uint dstW = consoleSize.x;
        uint dstH = consoleSize.y;

        static const char gradient[] = " .:!/r(l1Z4H9W8$@";
        static const int gradientSize = static_cast<int>(std::size(gradient)) - 2; // substract 2 for exclude '\0' symbol :D

        std::vector<char> charBuffer(dstW * dstH);

        float scaleX = static_cast<float>(srcW) / dstW;
        float scaleY = static_cast<float>(srcH) / dstH;

        for (uint y = 0; y < dstH; ++y)
        {
            uint srcYStart = static_cast<uint>(y * scaleY);
            uint srcYEnd = (y == dstH - 1) ? srcH : static_cast<uint>((y + 1) * scaleY);
            if (srcYEnd == 0) srcYEnd = 1;
            uint srcYCount = srcYEnd - srcYStart;

            for (uint x = 0; x < dstW; ++x)
            {
                uint srcXStart = static_cast<uint>(x * scaleX);
                uint srcXEnd = (x == dstW - 1) ? srcW : static_cast<uint>((x + 1) * scaleX);
                if (srcXEnd == 0) srcXEnd = 1;
                uint srcXCount = srcXEnd - srcXStart;

                float rSum = 0, gSum = 0, bSum = 0;
                uint samples = 0;

                for (uint sy = srcYStart; sy < srcYEnd && sy < srcH; ++sy)
                {
                    for (uint sx = srcXStart; sx < srcXEnd && sx < srcW; ++sx)
                    {
                        __m128 color = Read(uint2(sx, sy));
                        float rgba[4];
                        _mm_storeu_ps(rgba, color);
                        rSum += rgba[0];
                        gSum += rgba[1];
                        bSum += rgba[2];
                        ++samples;
                    }
                }

                if (samples > 0)
                {
                    rSum /= samples;
                    gSum /= samples;
                    bSum /= samples;

                    float luminance = 0.2126f * rSum + 0.7152f * gSum + 0.0722f * bSum;
                    luminance = AfterMath::clamp(luminance, 0.0f, 1.0f);

                    // Индекс в градиенте
                    int idx = static_cast<int>(luminance * gradientSize);
                    idx = AfterMath::clamp(idx, 0, gradientSize);
                    charBuffer[x + y * dstW] = gradient[idx];
                }
                else
                {
                    charBuffer[x + y * dstW] = ' ';
                }
            }
        }

        DWORD written = 0;
        COORD coord = { 0, 0 };
        WriteConsoleOutputCharacterA(hConsole, charBuffer.data(),
                                    static_cast<DWORD>(dstW * dstH),
                                    coord, &written);
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

        file.write(reinterpret_cast<const char*>(pixelsStorage.data()), pixelsStorage.size() * 4);
        file.close();
        return true;
    }

private:
    uint2 resolution;
    std::vector<uint32_t> pixelsStorage;   // BGRA
};

SOFTX_END
/////////////////////////////////////////////////////////////////
