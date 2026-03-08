#pragma once

#include <windows.h>
#include <vector>
#include <functional>
#include <memory>
#include <cassert>

#include "LibInternal.h"
#include "Texture.h"
#include "ThirdPartyIncluding.h"

SOFTX_BEGIN

// Presentation parameters
struct PresentParameters
{
    uint2 BackBufferSize;
    HWND hDeviceWindow;
    bool Windowed;
};

// Input vertex structure (model space)
struct VertexInput
{
    float3 Position;
    float3 Normal;
    float4 Color;
    float2 UV;

    VertexInput() : Position(0, 0, 0), Normal(0, 0, 0), Color(0, 0, 0, 0), UV(0, 0) {}
    VertexInput(const float3& pos, const float3& norm, const float4& col, const float2& uv = float2(0, 0))
        : Position(pos), Normal(norm), Color(col), UV(uv) {}
};

// Output vertex structure (screen space after VS)
struct VertexOutput
{
    float4 Position;
    float4 Color;
    float3 Normal;
    float2 UV;

    VertexOutput() : Position(0, 0, 0, 0), Normal(0, 0, 0), Color(0, 0, 0, 0), UV(0, 0) {}
    VertexOutput(const float4& pos, const float3& norm, const float4& col, const float2& uv = float2(0, 0))
        : Position(pos), Normal(norm), Color(col), UV(uv) {}
};

// Vertex buffer
class VertexBuffer
{
public:
    using VertexData = std::vector<VertexInput>;

    VertexBuffer() = default;
    explicit VertexBuffer(std::shared_ptr<const VertexData> data) : data(std::move(data)) {}
    explicit VertexBuffer(const VertexData& data) : data(std::make_shared<const VertexData>(data)) {}
    VertexBuffer(std::initializer_list<VertexInput> list) : data(std::make_shared<const VertexData>(list)) {}

    size_t Size() const { return data ? data->size() : 0; }
    bool IsEmpty() const { return !data || data->empty(); }

    const VertexInput& GetByIndex(uint index) const
    {
        assert(data && index < data->size());
        return (*data)[index];
    }

    const VertexData* operator->() const { return data.get(); }
    const VertexData& operator*() const { return *data; }

private:
    std::shared_ptr<const VertexData> data;
};

// Index buffer
class IndexBuffer
{
public:
    using IndexData = std::vector<uint>;

    IndexBuffer() = default;
    explicit IndexBuffer(std::shared_ptr<const IndexData> data) : data(std::move(data)) {}
    explicit IndexBuffer(const IndexData& data) : data(std::make_shared<const IndexData>(data)) {}
    IndexBuffer(std::initializer_list<uint> list) : data(std::make_shared<const IndexData>(list)) {}

    size_t Size() const { return data ? data->size() : 0; }
    bool IsEmpty() const { return !data || data->empty(); }

    uint GetByIndex(uint index) const
    {
        assert(data && index < data->size());
        return (*data)[index];
    }

private:
    std::shared_ptr<const IndexData> data;
};

// Constant buffer (raw bytes)
class ConstantBuffer
{
public:
    using CBufferData = std::vector<char>;

    ConstantBuffer() = default;
    explicit ConstantBuffer(std::shared_ptr<const CBufferData> data) : data(std::move(data)) {}
    ConstantBuffer(const void* srcData, size_t size)
        : data(std::make_shared<const CBufferData>(
            static_cast<const char*>(srcData),
            static_cast<const char*>(srcData) + size))
    {}

    size_t Size() const { return data ? data->size() : 0; }
    const void* Data() const { return data ? data->data() : nullptr; }

private:
    std::shared_ptr<const CBufferData> data;
};

// Viewport description
struct Viewport
{
    float2 pos;
    int2 size;
    float minZ;
    float maxZ;

    Viewport() : pos(0.0f, 0.0f), size(0, 0), minZ(0), maxZ(1) {}
    Viewport(float x, float y, int width, int height, float minZ = 0, float maxZ = 1)
        : pos(x, y), size(width, height), minZ(minZ), maxZ(maxZ) {}
    Viewport(float2 pos, int2 size, float minZ = 0, float maxZ = 1)
        : pos(pos), size(size), minZ(minZ), maxZ(maxZ) {}
};

// Tile for tiled rendering
struct Tile
{
    uint2 min;
    uint2 max;
    std::vector<int> triangleIndices;

    Tile(uint2 min, uint2 max) : min(min), max(max)
    {
    }
};

// Sampler state
enum class Filter
{
    Nearest,
    Bilinear
};

enum class Wrap
{
    Repeat,
    Clamp,
    Mirror
};

struct SamplerState
{
    Filter filter = Filter::Bilinear;
    Wrap wrapU = Wrap::Repeat;
    Wrap wrapV = Wrap::Repeat;

    float ApplyWrap(float uv, Wrap mode) const
    {
        switch (mode)
        {
        case Wrap::Clamp:
            return std::clamp(uv, 0.0f, 1.0f);
        case Wrap::Mirror:
            {
                float t = std::fmod(std::abs(uv), 2.0f);
                return t > 1.0f ? 2.0f - t : t;
            }
        default: // Repeat
            return uv - std::floor(uv);
        }
    }

    float2 ApplyWrap(float2 uv) const
    {
        return { ApplyWrap(uv.x, wrapU), ApplyWrap(uv.y, wrapV) };
    }
};

// Texture binding (texture + sampler)
struct TextureBinding
{
private:
    const TextureRGBA32F* texture = nullptr;
    SamplerState sampler;

public:
    bool IsValid() const { return texture != nullptr; }
    bool IsEmpty() const { return !IsValid(); }

    void SetTexture(const TextureRGBA32F* tex) { texture = tex; }
    void SetSamplerState(SamplerState samp) { sampler = samp; }

    float4 Sample(float2 uv) const
    {
        if (!texture)
            return float4(1.0f, 0.0f, 1.0f, 1.0f); // magenta

        float2 wrapped = sampler.ApplyWrap(uv);

        if (sampler.filter == Filter::Bilinear)
            return texture->SampleBilinear(wrapped);
        else
            return texture->Sample(wrapped);
    }

    int2 GetDimensions() const
    {
        return int2(texture->Width(), texture->Height());
    }
};

// Texture table (fixed max slots)
static constexpr int MAX_TEXTURE_SLOTS = 16;

struct TextureTable
{
    TextureBinding bindings[MAX_TEXTURE_SLOTS];

    TextureBinding& operator[](int i)
    {
        assert(i >= 0 && i < MAX_TEXTURE_SLOTS);
        return bindings[i];
    }

    const TextureBinding& operator[](int i) const
    {
        assert(i >= 0 && i < MAX_TEXTURE_SLOTS);
        return bindings[i];
    }
};

// Shader function types
using PixelShader = std::function<float4(const VertexOutput& input, ConstantBuffer constantBuffer, const TextureTable& tex)>;
using VertexShader = std::function<VertexOutput(const VertexInput& input, ConstantBuffer constantBuffer, const TextureTable& tex)>;
using GeometryShader = std::function<void(const VertexOutput[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices, const TextureTable& tex)>;

// Rasterizer states
enum class CullMode
{
    None,
    Front,
    Back
};

enum class FillMode
{
    Point,
    Wireframe,
    Solid
};

enum class ComparisonFunc
{
    Never,      // always false
    Less,       // <
    Equal,      // ==
    LessEqual,  // <=
    Greater,    // >
    NotEqual,   // !=
    GreaterEqual, // >=
    Always      // always true
};

SOFTX_END
