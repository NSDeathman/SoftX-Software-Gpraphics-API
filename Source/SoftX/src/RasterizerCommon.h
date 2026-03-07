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

// Perspective-correct интерполяция атрибутов треугольника.
//
// Линейная интерполяция в экранном пространстве даёт неверный результат
// потому что перспективное деление нелинейно — равные шаги по экрану
// не соответствуют равным шагам в 3D пространстве.
//
// Формула:
//   A = (α * A0*invW0 + β * A1*invW1 + γ * A2*invW2) / (α*invW0 + β*invW1 + γ*invW2)
//
// где invW0/1/2 = Position.w каждой вершины (установлен в ClipSpaceToScreenSpace),
// α, β, γ — барицентрические координаты в экранном пространстве.
inline VertexOutput trilerp(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float alpha,
							float beta, float gamma)
{
	// Position.w хранит 1/w — взвешиваем каждую вершину
	float w0 = alpha * v0.Position.w;
	float w1 = beta * v1.Position.w;
	float w2 = gamma * v2.Position.w;

	float invWsum = w0 + w1 + w2;
	float wsum = (std::abs(invWsum) > 1e-10f) ? (1.0f / invWsum) : 0.0f;

	VertexOutput result;

#define PLERP(field) result.field = (w0 * v0.field + w1 * v1.field + w2 * v2.field) * wsum
	PLERP(Color);
	PLERP(Normal);
	PLERP(UV);
#undef PLERP

	// Position.xyz — линейно, perspective correction не нужна
	result.Position.x = alpha * v0.Position.x + beta * v1.Position.x + gamma * v2.Position.x;
	result.Position.y = alpha * v0.Position.y + beta * v1.Position.y + gamma * v2.Position.y;
	result.Position.z = alpha * v0.Position.z + beta * v1.Position.z + gamma * v2.Position.z;
	result.Position.w = invWsum; // интерполированный 1/w доступен в PS
	return result;
}

// Преобразует вершину из clip space в экранное пространство.
// Записывает 1/w в Position.w для perspective-correct интерполяции
// в растеризаторе (DX11-стиль: после растеризации Position.w == 1/w).
inline void ClipSpaceToScreenSpace(VertexOutput& vert, const Viewport& vp)
{
	float w = vert.Position.w;
	float invW = (std::abs(w) > 1e-10f) ? (1.0f / w) : 0.0f;

	float xNDC = vert.Position.x * invW;
	float yNDC = vert.Position.y * invW;
	float zNDC = vert.Position.z * invW;

	vert.Position.x = vp.pos.x + (xNDC * 0.5f + 0.5f) * vp.size.x;
	vert.Position.y = vp.pos.y + (1.0f - (yNDC * 0.5f + 0.5f)) * vp.size.y;
	vert.Position.z = vp.minZ + zNDC * (vp.maxZ - vp.minZ);
	vert.Position.w = invW; // DX11-стиль: Position.w = 1/w после растеризации
}

} // namespace RasterizerCommon

SOFTX_END
