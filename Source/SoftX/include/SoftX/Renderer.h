#pragma once

#include "LibInternal.h"
#include "RasterizerInterface.h"

SOFTX_BEGIN

class Renderer
{
public:
    Renderer(IRasterizer& rasterizer, 
             IRenderTarget& renderTarget, 
             DepthBuffer& depthBuffer, 
             const PixelShader& ps,
             const ConstantBuffer& cb, 
             const TextureTable* tt, 
             const RasterizerState& state, 
             uint tileSize);

    void Execute(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles);

    const std::vector<Tile>& GetTiles() const
    {
        return m_Tiles;
    }

private:
    void buildTiles(int width, int height);
    void binTriangles(const std::vector<VertexOutput>& verts, const std::vector<int3>& triangles);
    void renderTiles();
    void renderTile(int tileIndex);

    IRasterizer& m_Rasterizer;
    IRenderTarget& m_RenderTarget;
    DepthBuffer& m_DepthBuffer;
    const PixelShader& m_PS;
    const ConstantBuffer& m_CB;
    const TextureTable* m_TT;
    RasterizerState m_State;
    uint m_TileSize;

    std::vector<Tile> m_Tiles;
    const std::vector<VertexOutput>* m_Verts = nullptr;
    const std::vector<int3>* m_Triangles = nullptr;
};

SOFTX_END
