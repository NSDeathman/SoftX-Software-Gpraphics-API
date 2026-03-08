# SoftX – Software Graphics API

SoftX is a modern, high-performance software graphics API designed for x86/x64 platforms.  
It provides a DirectX‑style programming model with full support for **vertex**, **geometry**, and **pixel shaders**, while leveraging **tile‑based multithreaded rendering** and **SIMD acceleration** (SSE/AVX) for maximum efficiency.

## Key Features

- **DirectX‑inspired pipeline** – familiar concepts: device, immediate/deferred contexts, shaders, constant buffers, texture bindings.
- **Tile‑based rendering** – automatic binning of triangles into screen tiles, enabling efficient parallelization.
- **Multithreading** – create multiple deferred contexts to record command lists concurrently, then execute them in parallel.
- **SIMD optimised rasterisers** – scalar, SSE, and AVX backends selected at runtime based on CPU capabilities.
- **Shader support** – C++ callable objects (std::function) for vertex, geometry and pixel shaders; easy to integrate custom shading logic.
- **Math library included** – **Presence AfterMath**, a HLSL‑friendly mathematics library with row‑major matrices and left‑handed coordinate space (identical to DirectX conventions). Low learning curve for HLSL developers.
- **Dynamic link library** – compiled as a `.lib` import library with a DLL runtime (CRT linked dynamically).
- **Wide compiler support** – compatible with C++14, C++17, and C++20 standards.

## Architecture

SoftX is designed from the ground up for performance and flexibility:

- **Device** – creates resources, owns the backbuffer and depth buffer.
- **Immediate Context** – the main rendering channel; submits commands directly to the driver (i.e., the software rasteriser).
- **Deferred Contexts** – record rendering commands into command lists that can be executed later on the immediate context; perfect for multithreaded scene generation.
- **Rasteriser** – pluggable backend (scalar, SSE, AVX) automatically chosen by `CPUDetector`.
- **Tile‑based renderer** – splits the screen into tiles, bins triangles, and processes tiles in parallel using a thread pool.
- **Shader stages** – user‑supplied functors that follow a simple signature; no separate shader compilation required.

## Example

A minimal example of setting up a device, clearing the screen, and drawing a triangle can be found in the `examples/` folder.

```cpp
#include <SoftX/SoftX.h>

// Vertex and pixel shaders (lambdas)
auto vs = [](const VertexInput& v, ConstantBuffer, const TextureTable&) -> VertexOutput { ... };
auto ps = [](const VertexOutput& v, ConstantBuffer, const TextureTable&) -> float4 { ... };

// Create device
PresentParameters params = { {800,600}, hwnd, true };
Device device(params);

// Set shaders and resources
auto& ctx = device.GetImmediateContext();
ctx.SetVertexShader(vs);
ctx.SetPixelShader(ps);
ctx.SetVertexBuffer(myVertexBuffer);
ctx.SetIndexBuffer(myIndexBuffer);
ctx.SetRenderTarget(&device.GetBackBuffer());

// Draw
ctx.Clear(float4(0,0,0,1));
ctx.DrawIndexed();
device.Present();
```

## Building
SoftX is provided as a Visual Studio 2019 solution.

The library builds as SoftX.lib + SoftX.dll (dynamic CRT).

## The solution includes the AfterMath math library (header‑only).

Supported platforms: x86 and x64.

## C++ standard: C++14 (minimum), also tested with C++17 and C++20.

## License
SoftX is open‑source software released under the MIT License. See LICENSE for details.

For more information, please refer to the CONTRIBUTING guide and the example projects.
