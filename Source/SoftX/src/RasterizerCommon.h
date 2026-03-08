#pragma once

#include <SoftX/SoftX.h>

SOFTX_BEGIN

namespace RasterizerCommon
{
    inline float EdgeFunction(const float4& a, const float4& b, const float2& c)
    {
        return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
    }

    inline float EdgeFunction(const float4& a, const float4& b, const float4& c)
    {
        return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
    }

    // Perspective-correct interpolation of triangle attributes.
    //
    // Linear interpolation in screen space yields incorrect results
    // because perspective division is non-linear — equal steps on screen
    // do not correspond to equal steps in 3D space.
    //
    // Formula:
    //   A = (α * A0*invW0 + β * A1*invW1 + γ * A2*invW2) / (α*invW0 + β*invW1 + γ*invW2)
    //
    // where invW0/1/2 = Position.w of each vertex (set in ClipSpaceToScreenSpace),
    // and α, β, γ are barycentric coordinates in screen space.
    inline VertexOutput Trilerp(const VertexOutput& v0, 
                                const VertexOutput& v1, 
                                const VertexOutput& v2, 
                                float alpha,
                                float beta, 
                                float gamma)
    {
        // Position.w stores 1/w — weight each vertex
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

        // Position.xyz — linear, perspective correction not needed
#define LERP(field) result.field = (alpha * v0.field + beta * v1.field + gamma * v2.field)
        LERP(Position);
#undef LERP

        result.Position.w = invWsum; // interpolated 1/w available in PS

        return result;
    }

    // Transforms vertex from clip space to screen space.
    // Stores 1/w in Position.w for perspective-correct interpolation
    // in the rasterizer (DX11 style: after rasterization Position.w == 1/w).
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
        vert.Position.w = invW; // DX11 style: Position.w = 1/w after rasterization
    }

    // Linear interpolation of two vertices in clip space
    inline VertexOutput LerpVertexClipSpace(const VertexOutput& a, const VertexOutput& b, float t)
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

    // Clips triangle against near plane (w = nearW) in clip space.
    // Returns 0, 1 or 2 triangles in outTris[2][3].
    // Sutherland-Hodgman algorithm for a single plane.
    inline int ClipTriangleNearPlane(const VertexOutput& v0, 
                                     const VertexOutput& v1, 
                                     const VertexOutput& v2,
                                     VertexOutput outTris[2][3], 
                                     float nearW = 0.1f)
    {
        const VertexOutput* verts[3] = {&v0, &v1, &v2};

        bool inside[3];
        int insideCount = 0;
        for (int i = 0; i < 3; ++i)
        {
            inside[i] = verts[i]->Position.w >= nearW;
            if (inside[i])
                ++insideCount;
        }

        // All vertices behind camera — discard
        if (insideCount == 0) UNLIKELY
            return 0;

        // All vertices in front of camera — pass through unchanged
        if (insideCount == 3) LIKELY
        {
            outTris[0][0] = v0;
            outTris[0][1] = v1;
            outTris[0][2] = v2;
            return 1;
        }

        // Sutherland-Hodgman: traverse edges, build output polygon
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

            // Edge intersects plane — add intersection point
            if (aIn != bIn)
            {
                float wA = A.Position.w;
                float wB = B.Position.w;
                float t = (nearW - wA) / (wB - wA);
                poly[polySize++] = LerpVertexClipSpace(A, B, t);
            }
        }

        if (polySize < 3) UNLIKELY
            return 0;

        // Fan triangulation: 3 vertices → 1 triangle, 4 vertices → 2 triangles
        outTris[0][0] = poly[0];
        outTris[0][1] = poly[1];
        outTris[0][2] = poly[2];
        if (polySize == 4)
        {
            outTris[1][0] = poly[0];
            outTris[1][1] = poly[2];
            outTris[1][2] = poly[3];
            return 2;
        }
        return 1;
    }

} // namespace RasterizerCommon

SOFTX_END
