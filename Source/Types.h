#pragma once
#include <windows.h>
#include "MicroMath.h"  // для int2

struct PresentParameters {
    int2 BackBufferSize;       // размер заднего буфера (framebuffer)
    HWND hDeviceWindow;        // окно для вывода
    bool Windowed;             // всегда true для нашего софтверного рендерера
};

struct VertexInput {
    float3 Position;  // локальные координаты
    float4 Color;
};

struct VertexOutput {
    float4 Position;  // clip space coordinates (x,y,z,w)
    float4 Color;     // будет интерполироваться
    float2 UV;
};

struct Viewport {
    float x, y;          // верхний левый угол в пикселях
    float width, height; // размеры в пикселях
    float minZ, maxZ;    // диапазон глубины (обычно 0..1)

    Viewport() : x(0), y(0), width(0), height(0), minZ(0), maxZ(1) {}
    Viewport(float x, float y, float width, float height, float minZ = 0, float maxZ = 1)
        : x(x), y(y), width(width), height(height), minZ(minZ), maxZ(maxZ) {}
};

using PixelShader = std::function<float4(const VertexOutput& Input, const void* ConstantBuffer)>;
using VertexShader = std::function<VertexOutput(const VertexInput&, const void* ConstantBuffer)>;

enum class CullMode {
    None,   // не отсекать грани
    Front,  // отсекать лицевые грани
    Back    // отсекать тыльные грани (обычно используется)
};
