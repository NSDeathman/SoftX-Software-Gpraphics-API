/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "../include/LibInternal.h"
#include "../include/ThirdPartyIncluding.h"
#include "RasterizerCommon.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

/**
 * Manages a uniform grid of tiles covering the render target and
 * provides binning of triangles into the tiles they overlap.
 */
class TileGrid
{
public:
    TileGrid() = default;

    /**
     * Builds a grid of tiles for the given render target dimensions.
     * @param inWidth       Width of the render target in pixels.
     * @param inHeight      Height of the render target in pixels.
     * @param inTileSize    Side length of a square tile in pixels.
     */
    void Build(uint inWidth, uint inHeight, uint inTileSize)
    {
        width    = inWidth;
        height   = inHeight;
        tileSize = inTileSize;

        tilesX = (width + tileSize - 1) / tileSize;
        tilesY = (height + tileSize - 1) / tileSize;

        tiles.clear();
        tiles.reserve(tilesX * tilesY);

        for (uint ty = 0; ty < tilesY; ++ty)
        {
            for (uint tx = 0; tx < tilesX; ++tx)
            {
                uint2 mn(tx * tileSize, ty * tileSize);
                uint2 mx(std::min((tx + 1) * tileSize - 1, width - 1),
                         std::min((ty + 1) * tileSize - 1, height - 1));
                tiles.emplace_back(mn, mx);
            }
        }
    }

    /**
     * Assigns each triangle from the provided list to every tile
     * whose screen-space axis-aligned bounding box overlaps it.
     * @param setups   Pre-computed triangle setups (in screen space).
     */
    void BinTriangles(const std::vector<RasterizerCommon::TriangleSetup>& setups)
    {
        for (auto& tile : tiles)
            tile.triangleIndices.clear();

        if (tiles.empty() || setups.empty())
            return;

        const float renderTargetWidthFloat  = static_cast<float>(width)  - 1.0f;
        const float renderTargetHeightFloat = static_cast<float>(height) - 1.0f;

        const int tilesXInt = static_cast<int>(tilesX);
        const int tilesYInt = static_cast<int>(tilesY);
        const int tileSizeInt = static_cast<int>(tileSize);

        for (int setupIndex = 0; setupIndex < static_cast<int>(setups.size()); ++setupIndex)
        {
            const RasterizerCommon::TriangleSetup& setup = setups[setupIndex];
            const auto& v0 = setup.v0;
            const auto& v1 = setup.v1;
            const auto& v2 = setup.v2;

            float clampedX0 = AfterMath::clamp(v0.Position.x, 0.0f, renderTargetWidthFloat);
            float clampedY0 = AfterMath::clamp(v0.Position.y, 0.0f, renderTargetHeightFloat);
            float clampedX1 = AfterMath::clamp(v1.Position.x, 0.0f, renderTargetWidthFloat);
            float clampedY1 = AfterMath::clamp(v1.Position.y, 0.0f, renderTargetHeightFloat);
            float clampedX2 = AfterMath::clamp(v2.Position.x, 0.0f, renderTargetWidthFloat);
            float clampedY2 = AfterMath::clamp(v2.Position.y, 0.0f, renderTargetHeightFloat);

            float minX = std::min({ clampedX0, clampedX1, clampedX2 });
            float maxX = std::max({ clampedX0, clampedX1, clampedX2 });
            float minY = std::min({ clampedY0, clampedY1, clampedY2 });
            float maxY = std::max({ clampedY0, clampedY1, clampedY2 });

            if (minX >= renderTargetWidthFloat || maxX <= 0.0f ||
                minY >= renderTargetHeightFloat || maxY <= 0.0f)
                continue;

            int tileX0 = AfterMath::clamp(static_cast<int>(std::floor(minX)) / tileSizeInt, 0, tilesXInt - 1);
            int tileY0 = AfterMath::clamp(static_cast<int>(std::floor(minY)) / tileSizeInt, 0, tilesYInt - 1);
            int tileX1 = AfterMath::clamp(static_cast<int>(std::ceil(maxX)) / tileSizeInt, 0, tilesXInt - 1);
            int tileY1 = AfterMath::clamp(static_cast<int>(std::ceil(maxY)) / tileSizeInt, 0, tilesYInt - 1);

            for (int ty = tileY0; ty <= tileY1; ++ty)
                for (int tx = tileX0; tx <= tileX1; ++tx)
                    tiles[ty * tilesXInt + tx].triangleIndices.push_back(setupIndex);
        }
    }

    const std::vector<Tile>& GetTiles() const { return tiles; }

    uint GetWidth()    const { return width; }
    uint GetHeight()   const { return height; }
    uint GetTileSize() const { return tileSize; }
    uint GetTilesX()   const { return tilesX; }
    uint GetTilesY()   const { return tilesY; }

private:
    std::vector<Tile> tiles;
    uint width    = 0;
    uint height   = 0;
    uint tileSize = 0;
    uint tilesX   = 0;
    uint tilesY   = 0;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
