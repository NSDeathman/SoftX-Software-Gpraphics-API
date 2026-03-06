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

inline VertexOutput trilerp(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float a, float b, float c)
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

inline float4 ClipSpaceToScreenSpace(const float4& clipPos, const Viewport& vp)
{
	float invW = 1.0f / clipPos.w;
	float xNDC = clipPos.x * invW;
	float yNDC = clipPos.y * invW;
	float zNDC = clipPos.z * invW;

	float screenX = vp.pos.x + (xNDC * 0.5f + 0.5f) * vp.size.x;
	float screenY = vp.pos.y + (1.0f - (yNDC * 0.5f + 0.5f)) * vp.size.y;
	float screenZ = vp.minZ + zNDC * (vp.maxZ - vp.minZ);

	return float4(screenX, screenY, screenZ, 1.0f);
}
} // RasterizerCommon

SOFTX_END
