#pragma once

#include <SoftX/SoftX.h>

SOFTX_BEGIN

namespace RasterizerCommon
{
inline float edgeFunction(const float4& a, const float4& b, const float2& c)
{
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

inline float edgeFunction(const float4& a, const float4& b, const float4& c)
{
	return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

inline VertexOutput trilerp(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float a, float b,
							float c)
{
	VertexOutput result;

#define TRILERP(field) result.field = a * v0.field + b * v1.field + c * v2.field

	TRILERP(Position);
	TRILERP(Normal);
	TRILERP(Color);
	TRILERP(UV);

#undef TRILERP

	return result;
}
} // RasterizerCommon

SOFTX_END
