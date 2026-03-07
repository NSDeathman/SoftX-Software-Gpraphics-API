#pragma once

#include <windows.h>
#include <vector>
#include <functional>

#include "ThirdPartyIncluding.h"
#include "LibInternal.h"
#include "Texture.h"

SOFTX_BEGIN

struct PresentParameters
{
	int2 BackBufferSize;
	HWND hDeviceWindow;
	bool Windowed;
};

struct VertexInput
{
	float3 Position;
	float3 Normal;
	float4 Color;
	float2 UV;

	VertexInput() : Position(0, 0, 0), Normal(0, 0, 0), Color(0, 0, 0, 0), UV(0, 0)
	{
	}
	VertexInput(const float3& pos, const float3& norm, const float4& col, const float2& uv = float2(0, 0)) : Position(pos), Normal(norm), Color(col), UV(uv)
	{
	}
};

struct VertexOutput
{
	float4 Position;
	float4 Color;
	float3 Normal;
	float2 UV;

	VertexOutput() : Position(0, 0, 0, 0), Normal(0, 0, 0), Color(0, 0, 0, 0), UV(0, 0)
	{
	}
	VertexOutput(const float4& pos, const float3& norm, const float4& col, const float2& uv = float2(0, 0)) : Position(pos), Normal(norm), Color(col), UV(uv)
	{
	}
};

class VertexBuffer
{
  public:
	using VertexData = std::vector<VertexInput>;

	VertexBuffer() = default;
	explicit VertexBuffer(std::shared_ptr<const VertexData> data) : m_data(std::move(data))
	{
	}
	explicit VertexBuffer(const VertexData& data) : m_data(std::make_shared<const VertexData>(data))
	{
	}
	VertexBuffer(std::initializer_list<VertexInput> list) : m_data(std::make_shared<const VertexData>(list))
	{
	}

	size_t Size() const
	{
		return m_data ? m_data->size() : 0;
	}
	bool IsEmpty() const
	{
		return !m_data || m_data->empty();
	}

	const VertexInput& GetByIndex(uint32_t index) const
	{
		assert(m_data && index < m_data->size());
		return (*m_data)[index];
	}

	const VertexData* operator->() const
	{
		return m_data.get();
	}
	const VertexData& operator*() const
	{
		return *m_data;
	}

  private:
	std::shared_ptr<const VertexData> m_data;
};

class IndexBuffer
{
  public:
	using IndexData = std::vector<uint32_t>;

	IndexBuffer() = default;
	explicit IndexBuffer(std::shared_ptr<const IndexData> data) : m_data(std::move(data))
	{
	}
	explicit IndexBuffer(const IndexData& data) : m_data(std::make_shared<const IndexData>(data))
	{
	}
	IndexBuffer(std::initializer_list<uint32_t> list) : m_data(std::make_shared<const IndexData>(list))
	{
	}

	size_t Size() const
	{
		return m_data ? m_data->size() : 0;
	}
	bool IsEmpty() const
	{
		return !m_data || m_data->empty();
	}

	uint32_t GetByIndex(uint32_t index) const
	{
		assert(m_data && index < m_data->size());
		return (*m_data)[index];
	}

  private:
	std::shared_ptr<const IndexData> m_data;
};

class ConstantBuffer
{
  public:
	using CBufferData = std::vector<char>;

	ConstantBuffer() = default;
	explicit ConstantBuffer(std::shared_ptr<const CBufferData> data) : m_data(std::move(data))
	{
	}
	ConstantBuffer(const void* data, size_t size)
		: m_data(std::make_shared<const CBufferData>(static_cast<const char*>(data), static_cast<const char*>(data) + size))
	{
	}

	size_t Size() const
	{
		return m_data ? m_data->size() : 0;
	}
	const void* Data() const
	{
		return m_data ? m_data->data() : nullptr;
	}

  private:
	std::shared_ptr<const CBufferData> m_data;
};

struct Viewport
{
	float2 pos;
	int2 size;
	float minZ, maxZ;

	Viewport() : pos(float2(0.0f, 0.0f)), size(int2(0, 0)), minZ(0), maxZ(1)
	{
	}
	Viewport(float x, float y, int width, int height, float minZ = 0, float maxZ = 1)
		: pos(float2(x,y)), size(int2(width, height)), minZ(minZ), maxZ(maxZ)
	{
	}
	Viewport(float2 _pos, int2 _size, float minZ = 0, float maxZ = 1)
		: pos(_pos), size(_size), minZ(minZ), maxZ(maxZ)
	{
	}
};

struct Tile
{
	int2 min;
	int2 max;
	std::vector<int> triangleIndices;

	Tile(int2 min, int2 max) : min(min), max(max)
	{
	}
};

// ── Sampler state ─────────────────────────────────────────────────────────
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

	float applyWrap(float uv, Wrap mode) const
	{
		switch (mode)
		{
		case Wrap::Clamp:
			return std::clamp(uv, 0.0f, 1.0f);
		case Wrap::Mirror: {
			float t = std::fmod(std::abs(uv), 2.0f);
			return t > 1.0f ? 2.0f - t : t;
		}
		default:
			return uv - std::floor(uv); // Repeat
		}
	}

	float2 applyWrap(float2 uv) const
	{
		return {applyWrap(uv.x, wrapU), applyWrap(uv.y, wrapV)};
	}
};

// ── Один биндинг: текстура + самплер ─────────────────────────────────────
struct TextureBinding
{
  private:
	const TextureRGBA32F* m_texture = nullptr;
	SamplerState m_sampler;

  public:
	bool IsValid() const
	{
		return m_texture != nullptr;
	}

	bool IsEmpty() const
	{
		return !IsValid();
	}

	void SetTexture(const TextureRGBA32F* texture)
	{
		m_texture = texture;
	}

	void SetSemplerState(SamplerState sampler)
	{
		m_sampler = sampler;
	}

	float4 Sample(float2 uv) const
	{
		if (!m_texture)
			return float4(1.0f, 0.0f, 1.0f, 1.0f); // маджента

		float2 wrapped = m_sampler.applyWrap(uv);

		if (m_sampler.filter == Filter::Bilinear)
			return m_texture->sample_bilinear(wrapped);
		else
			return m_texture->sample(wrapped);
	}
};

// ── Таблица биндингов ─────────────────────────────────────────────────────
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

using PixelShader = std::function<float4(const VertexOutput& Input, ConstantBuffer ConstantBuffer, const TextureTable& tex)>;
using VertexShader = std::function<VertexOutput(const VertexInput&, ConstantBuffer ConstantBuffer, const TextureTable& tex)>;
using GeometryShader = std::function<void(const VertexOutput[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices, const TextureTable& tex)>;

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
	Never,		  // always false
	Less,		  // <
	Equal,		  // ==
	LessEqual,	  // <=
	Greater,	  // >
	NotEqual,	  // !=
	GreaterEqual, // >=
	Always		  // always true
};

SOFTX_END
