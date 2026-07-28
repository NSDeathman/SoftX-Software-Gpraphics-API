/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#include "RasterizerCommon.h"
#include "../include/SoftX.h"
#include "ThreadPoolManager.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

void DeviceContext::DrawDebugLine(Texture& rt, const RasterizerState& rasterState, int x0, int y0, int x1, int y1, const float4& color)
{
    int dx = std::abs(x1 - x0);
    int dy = -std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int x = x0, y = y0;
    int w = rt.Width();
    int h = rt.Height();

    while (true)
    {
        if (x >= 0 && y >= 0 && x < w && y < h)
        {
            __m128 col = _mm_set_ps(color.w, color.z, color.y, color.x);
            rt.StreamWrite(uint2(x, y), col);
        }
        if (x == x1 && y == y1) break;

        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
    }
}

void DeviceContext::DrawTileBorders(Texture& rt, uint tileSize)
{
    int w = rt.Width();
    int h = rt.Height();
    float4 borderColor(0.0f, 1.0f, 0.0f, 1.0f);
    RasterizerState dummyState;
    for (int x = tileSize; x < w; x += tileSize)
        DrawDebugLine(rt, dummyState, x, 0, x, h - 1, borderColor);
    for (int y = tileSize; y < h; y += tileSize)
        DrawDebugLine(rt, dummyState, 0, y, w - 1, y, borderColor);
}

void DeviceContext::DrawActiveTileBorders(Texture& rt, const RasterizerState& rasterState, uint tileSize, const std::vector<Tile>& tiles)
{
    float4 borderColor(0.0f, 1.0f, 0.0f, 1.0f);

    // Corner length — 25% of tile size, but not less than 4 pixels
    const int cornerLen = std::max(4, static_cast<int>(tileSize * 0.25f));

    for (const auto& tile : tiles)
    {
        if (!tile.triangleIndices.empty())
        {
            int x0 = tile.min.x, y0 = tile.min.y;
            int x1 = tile.max.x, y1 = tile.max.y;
            int cx = std::min(cornerLen, (x1 - x0) / 2);
            int cy = std::min(cornerLen, (y1 - y0) / 2);

            // ┌ top-left corner
            DrawDebugLine(rt, rasterState, x0, y0, x0 + cx, y0, borderColor); // horizontal
            DrawDebugLine(rt, rasterState, x0, y0, x0, y0 + cy, borderColor); // vertical

            // ┐ top-right corner
            DrawDebugLine(rt, rasterState, x1 - cx, y0, x1, y0, borderColor);
            DrawDebugLine(rt, rasterState, x1, y0, x1, y0 + cy, borderColor);

            // └ bottom-left corner
            DrawDebugLine(rt, rasterState, x0, y1, x0 + cx, y1, borderColor);
            DrawDebugLine(rt, rasterState, x0, y1 - cy, x0, y1, borderColor);

            // ┘ bottom-right corner
            DrawDebugLine(rt, rasterState, x1 - cx, y1, x1, y1, borderColor);
            DrawDebugLine(rt, rasterState, x1, y1 - cy, x1, y1, borderColor);
        }
    }
}

SOFTX_END
/////////////////////////////////////////////////////////////////
