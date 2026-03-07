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
inline VertexOutput trilerp(const VertexOutput& v0, const VertexOutput& v1, const VertexOutput& v2, float alpha, float beta, float gamma)
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
#define LERP(field) result.field = (alpha * v0.field + beta * v1.field + gamma * v2.field)
	LERP(Position);
#undef LERP

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

// Линейная интерполяция двух вершин в clip space
inline VertexOutput lerpVertexClipSpace(const VertexOutput& a, const VertexOutput& b, float t)
{
	VertexOutput r;

#define CSLERP(field) r.field = a.field + t * (b.field - a.field)
	CSLERP(Position);
	CSLERP(Color);
	CSLERP(Normal);
	CSLERP(UV);
#undef CSLERP

	return r;
}

// Обрезает треугольник по near plane (w = nearW) в clip space.
// Возвращает 0, 1 или 2 треугольника в outTris[2][3].
// Алгоритм Sutherland-Hodgman для одной плоскости.
inline int ClipTriangleNearPlane(
    const VertexOutput& v0,
    const VertexOutput& v1,
    const VertexOutput& v2,
    VertexOutput outTris[2][3],
    float nearW = 0.1f)
{
    const VertexOutput* verts[3] = { &v0, &v1, &v2 };

    bool inside[3];
    int insideCount = 0;
    for (int i = 0; i < 3; ++i)
    {
        inside[i] = verts[i]->Position.w >= nearW;
        if (inside[i]) ++insideCount;
    }

    // Все за камерой — отбрасываем
    if (insideCount == 0) return 0;

    // Все перед камерой — пропускаем без изменений
    if (insideCount == 3)
    {
        outTris[0][0] = v0; outTris[0][1] = v1; outTris[0][2] = v2;
        return 1;
    }

    // Sutherland-Hodgman: обходим рёбра, собираем выходной полигон
    VertexOutput poly[4];
    int polySize = 0;

    for (int i = 0; i < 3; ++i)
    {
        int j = (i + 1) % 3;
        const VertexOutput& A = *verts[i];
        const VertexOutput& B = *verts[j];
        bool aIn = inside[i];
        bool bIn = inside[j];

        if (aIn)
            poly[polySize++] = A;

        // Ребро пересекает плоскость — добавляем точку пересечения
        if (aIn != bIn)
        {
            float wA = A.Position.w;
            float wB = B.Position.w;
            float t  = (nearW - wA) / (wB - wA);
			poly[polySize++] = lerpVertexClipSpace(A, B, t);
        }
    }

    if (polySize < 3) return 0;

    // Fan triangulation: 3 вершины → 1 треугольник, 4 вершины → 2 треугольника
    outTris[0][0] = poly[0]; outTris[0][1] = poly[1]; outTris[0][2] = poly[2];
    if (polySize == 4)
    {
        outTris[1][0] = poly[0]; outTris[1][1] = poly[2]; outTris[1][2] = poly[3];
        return 2;
    }
    return 1;
}

} // namespace RasterizerCommon

SOFTX_END
