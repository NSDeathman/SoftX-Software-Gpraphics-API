/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

namespace RasterizerCommon
{
    // ─── Fixed-point sub-pixel rasterisation (28.4) ───────────────────────────
    //
    // Edge function at pixel P for edge A → B:
    //   E = (px − ax)·(by − ay) − (py − ay)·(bx − ax)  (all in fixed-point)
    //
    // Pixel x±1 step:  ΔE_x = S·(by − ay)   (int32 safe: ≤ 16 × 2·4096·16 ≈ 2²¹)
    // Row   y±1 step:  ΔE_y = −S·(bx − ax)  (same)
    // Initial value:   ≤ (2·4096·16)²        = 2³⁴  → int64 required
    //
    // int64→int32 cast in SIMD is lossless up to ~1920×1080 (SUBPIXEL_BITS=4).
    // For 4 K, set SUBPIXEL_BITS = 2.

    static constexpr int SUBPIXEL_BITS = 4;
    static constexpr int SUBPIXEL_STEP = 1 << SUBPIXEL_BITS; // 16

    // Screen-space float → fixed-point integer
    inline int ToFixed(float v)
    {
        return static_cast<int>(std::lround(v * float(SUBPIXEL_STEP)));
    }

    // Fixed-point coordinate of pixel-centre for pixel index i
    //   Pixel i occupies [i·S, (i+1)·S)  →  centre = i·S + S/2
    inline int PixelCentre(int i)
    {
        return i * SUBPIXEL_STEP + (SUBPIXEL_STEP >> 1);
    }

    // Integer edge function — int64 to avoid overflow during initial setup.
    // Units: SUBPIXEL_STEP² × float_edge_func (ratio f/area is preserved).
    inline int64_t EdgeFunctionInt(int ax, int ay, int bx, int by, int px, int py)
    {
        return int64_t(px - ax) * (by - ay) - int64_t(py - ay) * (bx - ax);
    }

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
    inline Interpolant Trilerp(const Interpolant& v0, 
                               const Interpolant& v1, 
                               const Interpolant& v2, 
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

        Interpolant result;

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
    inline void ClipSpaceToScreenSpace(Interpolant& vert, const Viewport& vp)
    {
        float w = vert.Position.w;
        float invW = (std::abs(w) > 1e-10f) ? (1.0f / w) : 0.0f;

        float xNDC = vert.Position.x * invW;
        float yNDC = vert.Position.y * invW;
        float zNDC = vert.Position.z * invW;

        vert.Position.x = vp.pos.x + (xNDC * 0.5f + 0.5f) * static_cast<float>(vp.size.x);
        vert.Position.y = vp.pos.y + (1.0f - (yNDC * 0.5f + 0.5f)) * static_cast<float>(vp.size.y);
        vert.Position.z = vp.minZ + zNDC * (vp.maxZ - vp.minZ);
        vert.Position.w = invW; // DX11 style: Position.w = 1/w after rasterization
    }

    // Linear interpolation of two vertices in clip space
    inline Interpolant LerpVertexClipSpace(const Interpolant& a, const Interpolant& b, float t)
    {
        Interpolant r;

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
    inline int ClipTriangleNearPlane(const Interpolant& v0,
                                     const Interpolant& v1,
                                     const Interpolant& v2,
                                     Interpolant outTris[2][3])
    {
        const Interpolant* verts[3] = { &v0, &v1, &v2 };
        bool inside[3];
        int insideCount = 0;

        for (int i = 0; i < 3; ++i) 
        {
            inside[i] = (verts[i]->Position.z >= 0.0f) && (verts[i]->Position.w > 0.0f);
            if (inside[i]) ++insideCount;
        }

        if (insideCount == 0) return 0;
        if (insideCount == 3) 
        {
            outTris[0][0] = v0; outTris[0][1] = v1; outTris[0][2] = v2;
            return 1;
        }

        Interpolant poly[4];
        int polySize = 0;

        for (int i = 0; i < 3; ++i) 
        {
            int j = (i + 1) % 3;
            const Interpolant& A = *verts[i];
            const Interpolant& B = *verts[j];
            bool aIn = inside[i];
            bool bIn = inside[j];

            if (aIn) poly[polySize++] = A;

            if (aIn != bIn) 
            {
                float t = (0.0f - A.Position.z) / (B.Position.z - A.Position.z);
                t = AfterMath::clamp(t, 0.0f, 1.0f);
                poly[polySize++] = LerpVertexClipSpace(A, B, t);
            }
        }

        if (polySize < 3) return 0;

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

    SOFTX_FORCE_INLINE bool DepthTest(float z, float depth, ComparisonFunc func)
    {
        switch (func) 
        {
        case ComparisonFunc::Never:        return false;
        case ComparisonFunc::Less:         return z < depth;
        case ComparisonFunc::Equal:        return z == depth;
        case ComparisonFunc::LessEqual:    return z <= depth;
        case ComparisonFunc::Greater:      return z > depth;
        case ComparisonFunc::NotEqual:     return z != depth;
        case ComparisonFunc::GreaterEqual: return z >= depth;
        case ComparisonFunc::Always:       return true;
        }
        return false;
    }

    struct TriangleSetup
    {
        // Fixed point (28.4) vert coords
        int x0fp, y0fp;
        int x1fp, y1fp;
        int x2fp, y2fp;

        int stepX01, stepX12, stepX20;
        int stepY01, stepY12, stepY20;

        // Precomputed pixel‑space bounding box (integer, floor/ceil)
        int bbMinX, bbMaxX;
        int bbMinY, bbMaxY;

        int64_t f01Base, f12Base, f20Base;

        float invArea2;      // 1.0 / (2 * area in fixed-point units)
        int   normSign;      // 1 (CCW) or -1 (flipped CW)

        Interpolant v0, v1, v2;
    };

    inline std::optional<TriangleSetup> CreateTriangleSetup(const Interpolant& a,
                                                            const Interpolant& b,
                                                            const Interpolant& c,
                                                            const RasterizerState& state)
    {
        // Convert to fixed point
        const int x0 = ToFixed(a.Position.x), y0 = ToFixed(a.Position.y);
        const int x1 = ToFixed(b.Position.x), y1 = ToFixed(b.Position.y);
        const int x2 = ToFixed(c.Position.x), y2 = ToFixed(c.Position.y);

        // Area (squared) and degeneracy test
        int64_t area2 = EdgeFunctionInt(x0, y0, x1, y1, x2, y2);
        if (area2 == 0) return std::nullopt;

        // Backface / frontface culling
        const CullMode cull = state.cullMode;
        if (cull == CullMode::Back && area2 > 0) return std::nullopt;
        if (cull == CullMode::Front && area2 < 0) return std::nullopt;

        // Normalizing: always CCW (area2 > 0)
        int normSign = (area2 > 0) ? 1 : -1;
        if (area2 < 0) area2 = -area2;

        TriangleSetup s;
        s.x0fp = x0; s.y0fp = y0;
        s.x1fp = x1; s.y1fp = y1;
        s.x2fp = x2; s.y2fp = y2;

        s.invArea2 = 1.0f / static_cast<float>(area2);
        s.normSign = normSign;

        s.stepX01 = normSign * SUBPIXEL_STEP * (y1 - y0);
        s.stepX12 = normSign * SUBPIXEL_STEP * (y2 - y1);
        s.stepX20 = normSign * SUBPIXEL_STEP * (y0 - y2);

        s.stepY01 = -normSign * SUBPIXEL_STEP * (x1 - x0);
        s.stepY12 = -normSign * SUBPIXEL_STEP * (x2 - x1);
        s.stepY20 = -normSign * SUBPIXEL_STEP * (x0 - x2);

        s.v0 = a;
        s.v1 = b;
        s.v2 = c;

        float minX = std::min({ a.Position.x, b.Position.x, c.Position.x });
        float maxX = std::max({ a.Position.x, b.Position.x, c.Position.x });
        float minY = std::min({ a.Position.y, b.Position.y, c.Position.y });
        float maxY = std::max({ a.Position.y, b.Position.y, c.Position.y });

        s.bbMinX = static_cast<int>(std::floor(minX));
        s.bbMaxX = static_cast<int>(std::ceil(maxX));
        s.bbMinY = static_cast<int>(std::floor(minY));
        s.bbMaxY = static_cast<int>(std::ceil(maxY));

        const int pcMinX = PixelCentre(s.bbMinX);
        const int pcMinY = PixelCentre(s.bbMinY);
        s.f01Base = normSign * EdgeFunctionInt(x0, y0, x1, y1, pcMinX, pcMinY);
        s.f12Base = normSign * EdgeFunctionInt(x1, y1, x2, y2, pcMinX, pcMinY);
        s.f20Base = normSign * EdgeFunctionInt(x2, y2, x0, y0, pcMinX, pcMinY);

        return s;
    }

    template <typename PixelFunc>
    void RasterizeTriangleImpl(const TriangleSetup& s,
                               uint2 tileMin, uint2 tileMax,
                               uint width,
                               PixelFunc&& processPixel)
    {
        // ── Bounding box clipping to tile ──────────────────────────────────────
        int bbMinX = std::max(static_cast<int>(tileMin.x), s.bbMinX);
        int bbMaxX = std::min(static_cast<int>(tileMax.x), s.bbMaxX);
        int bbMinY = std::max(static_cast<int>(tileMin.y), s.bbMinY);
        int bbMaxY = std::min(static_cast<int>(tileMax.y), s.bbMaxY);

        if (bbMinX > bbMaxX || bbMinY > bbMaxY) return;

        const int64_t dxTile = bbMinX - s.bbMinX;
        const int64_t dyTile = bbMinY - s.bbMinY;

        int64_t f01Tile = s.f01Base + dxTile * s.stepX01 + dyTile * s.stepY01;
        int64_t f12Tile = s.f12Base + dxTile * s.stepX12 + dyTile * s.stepY12;
        int64_t f20Tile = s.f20Base + dxTile * s.stepX20 + dyTile * s.stepY20;

        // Aliases for readability
        const int x0 = s.x0fp, y0 = s.y0fp;
        const int x1 = s.x1fp, y1 = s.y1fp;
        const int x2 = s.x2fp, y2 = s.y2fp;
        const int ns = s.normSign;
        const float invArea = s.invArea2;

        // ── Scanline traversal ───────────────────────────────────────────────
        //
        // Efficient for large triangles: incremental edge functions advance
        // by a fixed integer step per pixel / per row — no per-pixel multiply.
        //
        // The edge step deltas have been precomputed during TriangleSetup
        // and are reused here unchanged.

        // Row-start values at pixel centre (bbMinX, bbMinY)
        const int64_t dx = bbMinX - s.bbMinX;
        const int64_t dy = bbMinY - s.bbMinY;
        int64_t f01Row = s.f01Base + dx * s.stepX01 + dy * s.stepY01;
        int64_t f12Row = s.f12Base + dx * s.stepX12 + dy * s.stepY12;
        int64_t f20Row = s.f20Base + dx * s.stepX20 + dy * s.stepY20;

        for (int y = bbMinY; y <= bbMaxY; ++y, f01Row += s.stepY01, f12Row += s.stepY12, f20Row += s.stepY20)
        {
            int64_t f01 = f01Row;
            int64_t f12 = f12Row;
            int64_t f20 = f20Row;

            for (int x = bbMinX; x <= bbMaxX; ++x, f01 += s.stepX01, f12 += s.stepX12, f20 += s.stepX20)
            {
                // Single branch: OR of sign bits — negative if any f < 0
                if ((f01 | f12 | f20) < 0) continue;

                const float fa = float(f12) * invArea;
                const float fb = float(f20) * invArea;
                const float fc = float(f01) * invArea;

                processPixel(x, y, fa, fb, fc);
            }
        }
    }

} // namespace RasterizerCommon

SOFTX_END
/////////////////////////////////////////////////////////////////
