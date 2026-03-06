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
	int x = x0, y = y0;
	while (true)
	{
		if (x >= 0 && x < rt->width() && y >= 0 && y < rt->height())
		{
			rt->set_pixel(int2(x, y), color);
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
	int w = rt->width();
	int h = rt->height();
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

void DeviceContext::DrawActiveTileBorders()
{
	if (!m_RenderTarget)
		return;
	float4 borderColor(1.0f, 0.0f, 0.0f, 1.0f); // красный для активных тайлов
	for (const auto& tile : m_tiles)
	{
		if (!tile.triangleIndices.empty())
		{
			// Верхняя горизонтальная линия
			DrawDebugLine(tile.min.x, tile.min.y, tile.max.x, tile.min.y, borderColor);
			// Нижняя горизонтальная линия
			DrawDebugLine(tile.min.x, tile.max.y, tile.max.x, tile.max.y, borderColor);
			// Левая вертикальная линия
			DrawDebugLine(tile.min.x, tile.min.y, tile.min.x, tile.max.y, borderColor);
			// Правая вертикальная линия
			DrawDebugLine(tile.max.x, tile.min.y, tile.max.x, tile.max.y, borderColor);
		}
	}
}

SOFTX_END
