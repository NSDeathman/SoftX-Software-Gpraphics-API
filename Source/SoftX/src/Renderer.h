/////////////////////////////////////////////////////////////////
// SoftX - Software Graphics API
// Copyright (c) 2026 NSDeathman
// Licensed under the MIT License.
/////////////////////////////////////////////////////////////////
#pragma once
/////////////////////////////////////////////////////////////////
#include "../include/SoftX.h"
#include "../include/RasterizerInterface.h"
/////////////////////////////////////////////////////////////////
SOFTX_BEGIN

class Renderer
{
public:
    Renderer(IRasterizer& rasterizer);

    void Execute(const PipelineStateObject& pso, const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles);

    const std::vector<Tile>& GetTiles() const { return tiles; }

private:
    void BuildTiles(uint global_width, uint global_height, uint global_tileSize);
    void BinTriangles(const std::vector<VertexOutput>& verts, 
                      const std::vector<int3>& triangles,
                      uint width, 
                      uint height, 
                      uint tileSize);
    void RenderTiles(const PipelineStateObject& pso,
                     const std::vector<VertexOutput>& verts,
                     const std::vector<int3>& triangles);

    IRasterizer& rasterizer;
    uint width = 0;
    uint height = 0;
    uint tileSize = 0;
    std::vector<Tile> tiles;
};

SOFTX_END
/////////////////////////////////////////////////////////////////
