/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>
#include <xmmintrin.h>

#include "LibInternal.h"
#include "ThirdPartyIncluding.h"
#include "TextureInterface.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class SOFTX_API TextureRGBA32F : public ITexture
{
public:
    explicit TextureRGBA32F(uint2 size, uint numMips = 1)
        : resolution(size), mipChain(std::max(1u, numMips))
    {
        __m128 zero = _mm_setzero_ps();
        for (auto& level : mipChain) {
            const uint mipIndex = static_cast<uint>(&level - mipChain.data());
            const uint w = MipWidth(mipIndex);
            const uint h = MipHeight(mipIndex);
            level.resize(w * h);
            for (auto& p : level)
                _mm_store_ps(reinterpret_cast<float*>(&p), zero);
        }
    }

    SOFTX_FORCE_INLINE __m128* GetRawPixels(uint level = 0) {
        level = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        return mipChain[level].data();
    }
    SOFTX_FORCE_INLINE const __m128* GetRawPixels(uint level = 0) const {
        level = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        return mipChain[level].data();
    }

    SOFTX_FORCE_INLINE __m128 Read(uint2 coords) const {
        assert(coords.x < resolution.x&& coords.y < resolution.y);
        return mipChain[0][coords.y * resolution.x + coords.x];
    }

    SOFTX_FORCE_INLINE __m128 Read(uint index) const {
        assert(index < static_cast<uint>(mipChain[0].size()));
        return mipChain[0][index];
    }

    SOFTX_FORCE_INLINE __m128 SampleRaw(float2 uv) const {
        uint x = static_cast<uint>(uv.x * static_cast<float>(resolution.x));
        uint y = static_cast<uint>(uv.y * static_cast<float>(resolution.y));
        if (x >= resolution.x) x = resolution.x - 1;
        if (y >= resolution.y) y = resolution.y - 1;
        return mipChain[0][y * resolution.x + x];
    }

    SOFTX_FORCE_INLINE __m128 SampleBilinearRaw(float2 uv) const {
        return SampleBilinearRaw(uv, 0);
    }

    SOFTX_FORCE_INLINE __m128 SampleBilinearRaw(float2 uv, uint level) const {
        uint mipLevel = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        uint w = MipWidth(mipLevel);
        uint h = MipHeight(mipLevel);

        float fx = uv.x * static_cast<float>(w) - 0.5f;
        float fy = uv.y * static_cast<float>(h) - 0.5f;

        int x0 = static_cast<int>(std::floor(fx));
        int y0 = static_cast<int>(std::floor(fy));

        float tx = fx - static_cast<float>(x0);
        float ty = fy - static_cast<float>(y0);

        __m128 c00 = FetchRaw(x0, y0, mipLevel);
        __m128 c10 = FetchRaw(x0 + 1, y0, mipLevel);
        __m128 c01 = FetchRaw(x0, y0 + 1, mipLevel);
        __m128 c11 = FetchRaw(x0 + 1, y0 + 1, mipLevel);

        __m128 wtx = _mm_set1_ps(tx);
        __m128 wty = _mm_set1_ps(ty);
        __m128 one = _mm_set1_ps(1.0f);
        __m128 w1tx = _mm_sub_ps(one, wtx);
        __m128 w1ty = _mm_sub_ps(one, wty);

        __m128 w00 = _mm_mul_ps(w1tx, w1ty);
        __m128 w10 = _mm_mul_ps(wtx, w1ty);
        __m128 w01 = _mm_mul_ps(w1tx, wty);
        __m128 w11 = _mm_mul_ps(wtx, wty);

        return _mm_add_ps(
            _mm_add_ps(_mm_mul_ps(c00, w00), _mm_mul_ps(c10, w10)),
            _mm_add_ps(_mm_mul_ps(c01, w01), _mm_mul_ps(c11, w11)));
    }

    SOFTX_FORCE_INLINE void StreamWrite(uint2 coords, __m128 color) {
        StreamWrite(coords, color, 0);
    }

    SOFTX_FORCE_INLINE void StreamWrite(uint index, __m128 color) {
        assert(index < static_cast<uint>(mipChain[0].size()));
        _mm_stream_ps(reinterpret_cast<float*>(&mipChain[0][index]), color);
    }

    SOFTX_FORCE_INLINE void StreamWrite(uint2 coords, __m128 color, uint level) {
        uint mipLevel = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        uint w = MipWidth(mipLevel);
        uint index = coords.y * w + coords.x;
        _mm_stream_ps(reinterpret_cast<float*>(&mipChain[mipLevel][index]), color);
    }

    SOFTX_FORCE_INLINE uint MipWidth(uint level) const {
        level = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        return std::max(1u, resolution.x >> level);
    }

    SOFTX_FORCE_INLINE uint MipHeight(uint level) const {
        level = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        return std::max(1u, resolution.y >> level);
    }

    float4 Sample(float2 uv) const override {
        __m128 color = SampleRaw(uv);
        return float4(color);
    }

    float4 SampleBilinear(float2 uv) const override {
        return float4(SampleBilinearRaw(uv));
    }

    float4 SampleLevel(float2 uv, float lod) const override {
        uint level = static_cast<uint>(lod + 0.5f);
        level = std::max(0u, std::min(level, static_cast<uint>(mipChain.size()) - 1));
        return float4(SampleBilinearRaw(uv, level));
    }

    __m128 FetchRaw(int x, int y) const override {
        int2 coords = int2(x, y);
        coords.x = AfterMath::clamp(x, 0, static_cast<int>(resolution.x) - 1);
        coords.y = AfterMath::clamp(y, 0, static_cast<int>(resolution.y) - 1);
        return mipChain[0][static_cast<uint>(coords.y) * resolution.x + static_cast<uint>(coords.x)];
    }

    __m128 FetchRaw(int x, int y, uint level) const override {
        uint mipLevel = std::min(level, static_cast<uint>(mipChain.size()) - 1);
        uint w = MipWidth(mipLevel);
        uint h = MipHeight(mipLevel);
        int2 coords = int2(x, y);
        coords.x = AfterMath::clamp(x, 0, static_cast<int>(w) - 1);
        coords.y = AfterMath::clamp(y, 0, static_cast<int>(h) - 1);
        return mipChain[mipLevel][static_cast<uint>(coords.y) * w + static_cast<uint>(coords.x)];
    }

    uint Width() const override { return resolution.x; }
    uint Height() const override { return resolution.y; }
    uint MipCount() const override { return static_cast<uint>(mipChain.size()); }
    uint2 Size() const override { return resolution; }

    void GenerateMips();

    void SaveToTGA(const char* filename) const;

private:
    uint2 resolution;
    std::vector<std::vector<__m128>> mipChain;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
