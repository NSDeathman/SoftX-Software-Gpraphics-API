#include "pch.h"

#include <ppl.h>

#include <SoftX/SoftX.h>
#include <SoftX/ThreadPoolManager.h>
#include "RasterizerCommon.h"

SOFTX_BEGIN

void DeviceContext::DrawDebugLine(int x0, int y0, int x1, int y1, const float4& color)
{
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;
	int dx = std::abs(x1 - x0);
	int dy = -std::abs(y1 - y0);
	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;
	int err = dx + dy;
	uint x = x0, y = y0;
	while (true)
	{
		if (x >= 0 && x < rt->Width() && y >= 0 && y < rt->Height())
		{
			rt->SetPixel(int2(x, y), color);
		}
		if (x == x1 && y == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy)
		{
			err += dy;
			x += sx;
		}
		if (e2 <= dx)
		{
			err += dx;
			y += sy;
		}
	}
}

void DeviceContext::DrawTileBorders()
{
	IRenderTarget* rt = m_RenderTarget;
	if (!rt)
		return;
	int w = rt->Width();
	int h = rt->Height();
	float4 borderColor(0.0f, 1.0f, 0.0f, 1.0f);

	for (int x = m_TileSize; x < w; x += m_TileSize)
	{
		DrawDebugLine(x, 0, x, h - 1, borderColor);
	}
	for (int y = m_TileSize; y < h; y += m_TileSize)
	{
		DrawDebugLine(0, y, w - 1, y, borderColor);
	}
}

void DeviceContext::DrawActiveTileBorders(const std::vector<Tile>& tiles)
{
	if (!m_RenderTarget)
		return;

	float4 borderColor(0.0f, 1.0f, 0.0f, 1.0f);

	// Длина уголка — 25% от размера тайла, но не меньше 4 пикселей
	const int cornerLen = std::max(4, (int)(m_TileSize * 0.25f));

	for (const auto& tile : tiles)
	{
		if (!tile.triangleIndices.empty())
		{
			int x0 = tile.min.x, y0 = tile.min.y;
			int x1 = tile.max.x, y1 = tile.max.y;
			int cx = std::min(cornerLen, (x1 - x0) / 2);
			int cy = std::min(cornerLen, (y1 - y0) / 2);

			// ┌ верхний левый
			DrawDebugLine(x0, y0, x0 + cx, y0, borderColor); // горизонталь
			DrawDebugLine(x0, y0, x0, y0 + cy, borderColor); // вертикаль

			// ┐ верхний правый
			DrawDebugLine(x1 - cx, y0, x1, y0, borderColor);
			DrawDebugLine(x1, y0, x1, y0 + cy, borderColor);

			// └ нижний левый
			DrawDebugLine(x0, y1, x0 + cx, y1, borderColor);
			DrawDebugLine(x0, y1 - cy, x0, y1, borderColor);

			// ┘ нижний правый
			DrawDebugLine(x1 - cx, y1, x1, y1, borderColor);
			DrawDebugLine(x1, y1 - cy, x1, y1, borderColor);
		}
	}
}

SOFTX_END
