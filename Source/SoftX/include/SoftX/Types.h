#pragma once

#include <windows.h>
#include <vector>
#include <functional>

#include "ThirdPartyIncluding.h"
#include "LibInternal.h"

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
private:
	std::vector<VertexInput> m_data = {};

public:
	VertexBuffer()
	{
		m_data = {};
	}
	VertexBuffer(const std::vector<VertexInput>& data)
	{
		m_data = data;
	}
	size_t Size()
	{
		return m_data.size();
	}
	void Add(const VertexInput& Vertex)
	{
		m_data.push_back(Vertex);
	}
	void Clear()
	{
		m_data.clear();
	}
	bool IsEmpty() const
	{
		return m_data.empty();
	}
	VertexInput GetByIndex(uint32_t index)
	{
		return m_data[index];
	}
};

class IndexBuffer
{
private:
	std::vector<uint32_t> m_data = {};

public:
	IndexBuffer()
	{
		m_data = {};
	}
	IndexBuffer(const std::vector<uint32_t>& data)
	{
		m_data = data;
	}
	size_t Size()
	{
		return m_data.size();
	}
	void Add(const uint32_t& Index)
	{
		m_data.push_back(Index);
	}
	void Clear()
	{
		m_data.clear();
	}
	bool IsEmpty() const
	{
		return m_data.empty();
	}
	uint32_t GetByIndex(uint32_t index)
	{
		return m_data[index];
	}
};

class ConstantBuffer
{
private:
	const void* m_data;
	size_t m_size;

public:
	ConstantBuffer()
	{
		m_data = {};
		m_size = 0;
	}
	ConstantBuffer(const void* data, size_t size)
	{
		m_data = data;
		m_size = size;
	}
	size_t Size()
	{
		return m_size;
	}
	const void* Data()
	{
		return m_data;
	}
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

using PixelShader = std::function<float4(const VertexOutput& Input, ConstantBuffer ConstantBuffer)>;
using VertexShader = std::function<VertexOutput(const VertexInput&, ConstantBuffer ConstantBuffer)>;
using GeometryShader = std::function<void(const VertexOutput[3], std::vector<VertexOutput>& outVerts, std::vector<int>& outIndices)>;

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

SOFTX_END
