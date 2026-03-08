---
# SoftX C++ Coding Standard
## This document defines the coding style and conventions for the SoftX project.
## The primary inspirations are:

### Unreal Engine coding style (brace placement, naming, namespace macros).

### C++ Standard Library best practices (clarity, no abbreviations, modern language features).
---
# All new code must conform to these guidelines to ensure consistency and readability across the codebase.

## 1. Naming
### 1.1. General Principles
Use full, meaningful names without abbreviations.
Bad: btn, calc, tmp
Good: button, calculateSum, temporaryBuffer

English is used for all identifiers.

Use the types provided by the AfterMath library for vectors:
int2, uint2, float2, float3, float4. These are used throughout the project for coordinates, colors, etc.

### 1.2. Namespaces
Namespace names use PascalCase.
Use macros to explicitly mark the beginning and end of a namespace:

```cpp
#define SOFTX_BEGIN namespace SoftX {
#define SOFTX_END   }
```

Example:

```cpp
SOFTX_BEGIN

// ... code inside the SoftX namespace

SOFTX_END
```

### 1.3. Classes, Structures, Enumerations
Type names: PascalCase without prefixes (Unreal‑style prefixes like F, U, etc., may be added only if explicitly agreed upon).

Enumerations: type name in PascalCase, enumerators in PascalCase.

```cpp
class Renderer { ... };
struct VertexData { ... };
enum class BlendMode { Normal, Additive, Multiply };
```

### 1.4. Methods and Functions
PascalCase for all methods (including static and global functions).

Use verbs or verb phrases that describe the action.

```cpp
void InitializeEngine();
int CalculateFrameRate() const;
void SetViewportSize(int width, int height);
```

### 1.5. Class/Struct Fields
camelCase without prefixes (no m_ or trailing underscores).
This improves readability and avoids Hungarian notation.

```cpp
class Player
{
public:
    // ...
private:
    std::string playerName;
    float health{100.0f};
};
```

Example from the project (DepthBuffer):

```cpp
class DepthBuffer
{
    // ...
private:
    uint2 resolution;           // width/height as uint2
    uint widthPadded;           // padded width (multiple of 4)
    std::vector<__m128> blocks; // depth data in SIMD blocks
};
```

### 1.6. Local Variables and Function Parameters
camelCase (starts with a lowercase letter).

```cpp
void ProcessInput(float deltaTime)
{
    float movementSpeed = 10.0f;
    int frameCount = 0;
}
```

### 1.7. Macros and Compile-Time Constants
UPPER_SNAKE_CASE.

```cpp
#define SOFTX_API
#define MAX_BUFFER_SIZE 4096
constexpr int DEFAULT_PORT = 8080;
```

Project‑specific macros:

SOFTX_BEGIN / SOFTX_END – delimit the SoftX namespace.

LIKELY / UNLIKELY – branch prediction hints (expand to [[likely]]/[[unlikely]] in C++20).

PROFILE_SCOPE(name) – profiling annotation (expands to Optick macro or nothing).

## 2. Formatting
### 2.1. Braces
Opening braces always on a new line (Allman style).

```cpp
void MyFunction()
{
    if (condition)
    {
        // body
    }
    else
    {
        // body
    }
}
```

### 2.2. Indentation
Use 4 spaces (no tabs).

Indent nested namespaces and classes.

### 2.3. Spaces
One space between a keyword and the opening parenthesis:
if (condition), for (int i = 0; i < count; ++i)

No spaces inside parentheses: (x + y) * z, func(a, b)

Binary operators are surrounded by spaces: a + b, x == y

### 2.4. Line Length
Recommended maximum line length: 120 characters.

When breaking function arguments, place each new argument on a new line, indented.

```cpp
void LongFunctionName(int parameter1,
                      int parameter2,
                      float parameter3);
```
                      
### 2.5. Alignment of SIMD Data
Use alignas(16) or alignas(32) for variables that will be used with SSE/AVX intrinsics.

```cpp
alignas(16) float zArr[4];
```

## 3. Comments
### 3.1. Language
All comments must be written in English.

### 3.2. Documentation Comments
For public APIs use Doxygen style (/// or /** ... */).

```cpp
/**
 * Initializes the graphics engine.
 * @param width  Window width.
 * @param height Window height.
 * @return true on success, false on error.
 */
bool InitializeEngine(int width, int height);
```

### 3.3. Inline Comments
Use // for single‑line comments.

Comment complex or non‑obvious code, but avoid stating the obvious.

```cpp
// Switch state only after input validation.
if (IsDataValid(input))
{
    SwitchState(input);
}
```

## 4. Macros and Preprocessor Directives
The macros SOFTX_BEGIN and SOFTX_END are used strictly to delimit the namespace content.

Define macros in header files with proper include guards (#pragma once is preferred).

Use SOFTX_API for symbol export/import, defined according to platform/compiler.

Use LIKELY/UNLIKELY for branch prediction hints.

```cpp
// Example definition of SOFTX_API for Windows
#ifdef _WIN32
    #ifdef SOFTX_BUILD_DLL
        #define SOFTX_API __declspec(dllexport)
    #else
        #define SOFTX_API __declspec(dllimport)
    #endif
#else
    #define SOFTX_API
#endif
```

## 5. Code Organization
### 5.1. Header Files
Header file name should match the class/module name: Renderer.h, MathUtils.h.

Each header must start with #pragma once.

Include only what is necessary; prefer forward declarations.

### 5.2. Implementation Files (.cpp)
Include order:

Corresponding header (e.g., Renderer.cpp starts with #include "Renderer.h").

Other project headers.

External library headers.

Standard library headers.

Separate each group with a blank line.

Example (Device.cpp):

```cpp
#include "pch.h"

#include <SoftX/Device.h>
#include <SoftX/SoftX.h>

SOFTX_BEGIN

// ... implementation

SOFTX_END
```

### 5.3. Namespaces
All code must reside inside SOFTX_BEGIN / SOFTX_END (i.e., within namespace SoftX).

For nested namespaces use similar macros or explicit syntax.

## 6. Encoding and Character Set
All source files must be saved with UTF-8 encoding.

Stick to regular Unicode characters (primarily ASCII and characters from the Basic Multilingual Plane). Avoid invisible or special Unicode characters (e.g., non‑breaking spaces, directional markers, control characters) that could cause encoding or tooling issues.

## 7. Additional Recommendations
### 7.1. Use of const
Mark methods that do not modify the object as const.

Use const for variables that should not change after initialization.

Prefer const T& for parameters instead of copying unless modification is needed.

### 7.2. Initialization
Use uniform initialization ({}) to prevent narrowing conversions.

```cpp
int value{42};
std::vector<int> numbers{1, 2, 3};
```

### 7.3. Error Handling
Exceptions are used only in critical cases (team agreement required).

For code that must not throw, use error codes or std::optional.

### 7.4. Modern C++
The project targets C++17 (or later). Prefer standard library facilities over manual resource management.

Use std::unique_ptr, std::shared_ptr for ownership.

### 7.5. SIMD and Performance
When using SSE/AVX intrinsics, ensure data is properly aligned (alignas(16) or alignas(32)).

Use streaming stores (_mm_stream_ps) for large writes to bypass cache.

Prefer incremental edge function updates in rasterizers to reduce computation.

Use the profiling macros (PROFILE_SCOPE) to mark performance-critical sections.

7.6. Unsigned Types for Coordinates
Use uint, uint2 for pixel coordinates, tile indices, and sizes. This eliminates signed/unsigned mismatches and naturally reflects non‑negative values.

## 8. Code Example
Below are real‑world examples from the SoftX project demonstrating the coding style.

DepthBuffer.h
```cpp
#pragma once

#include <xmmintrin.h>
#include <vector>
#include <cassert>

#include "LibInternal.h"
#include "ThirdPartyIncluding.h"

SOFTX_BEGIN

class SOFTX_API DepthBuffer
{
public:
    explicit DepthBuffer(uint2 size)
        : resolution(size)
        , widthPadded((size.x + 3) & ~3) // Round up to multiple of 4
        , blocks(widthPadded * size.y / 4)
    {
        Clear(1.0f);
    }

    void Clear(float depth)
    {
        __m128 depth4 = _mm_set1_ps(depth);
        __m128* ptr = blocks.data();
        size_t count = blocks.size();

        for (size_t i = 0; i < count; ++i)
            _mm_stream_ps(reinterpret_cast<float*>(ptr + i), depth4);
        _mm_sfence();
    }

    float Read(int2 coords) const
    {
        return FloatPtr()[coords.y * widthPadded + coords.x];
    }

    void Write(int2 coords, float depth)
    {
        FloatPtr()[coords.y * widthPadded + coords.x] = depth;
    }

    float& At(int2 coords)
    {
        return FloatPtr()[coords.y * widthPadded + coords.x];
    }

    const float& At(int2 coords) const
    {
        return FloatPtr()[coords.y * widthPadded + coords.x];
    }

    float& At(uint index)
    {
        uint x = index % resolution.x;
        uint y = index / resolution.x;
        return At(int2(x, y));
    }

    const float& At(uint index) const
    {
        uint x = index % resolution.x;
        uint y = index / resolution.x;
        return At(int2(x, y));
    }

    __m128 Read4(uint2 coords) const
    {
        assert(coords.x % 4 == 0);
        assert(coords.x + 3u < widthPadded && coords.y < resolution.y);
        uint blockIdx = (coords.y * widthPadded + coords.x) / 4u;
        return blocks[blockIdx];
    }

    void Write4(uint2 coords, __m128 depths, __m128 mask)
    {
        assert(coords.x % 4 == 0);
        assert(coords.x + 3u < widthPadded && coords.y < resolution.y);
        uint blockIdx = (coords.y * widthPadded + coords.x) / 4u;
        __m128& block = blocks[blockIdx];
        block = _mm_or_ps(_mm_and_ps(depths, mask),
                          _mm_andnot_ps(mask, block));
    }

    __m128 Test4(uint2 coords, __m128 depth4, __m128 activeMask) const
    {
        __m128 buffered = Read4(coords);
        __m128 passed = _mm_cmplt_ps(depth4, buffered);
        return _mm_and_ps(passed, activeMask);
    }

    uint Width() const { return resolution.x; }
    uint Height() const { return resolution.y; }
    uint WidthPadded() const { return widthPadded; }
    uint2 Size() const { return resolution; }

    float* Data() { return FloatPtr(); }
    const float* Data() const { return FloatPtr(); }

private:
    float* FloatPtr() { return reinterpret_cast<float*>(blocks.data()); }
    const float* FloatPtr() const { return reinterpret_cast<const float*>(blocks.data()); }

    uint2 resolution;
    uint widthPadded;
    std::vector<__m128> blocks;
};

SOFTX_END
```

RasterizerScalar.cpp (partial)

```cpp
#include "pch.h"
#include <SoftX/SoftX.h>
#include "RasterizerCommon.h"

SOFTX_BEGIN

void RasterizerScalar::RasterizeTriangle(
    const VertexOutput& v0,
    const VertexOutput& v1,
    const VertexOutput& v2,
    const RasterizerState& state,
    DepthBuffer& depthBuffer,
    IRenderTarget& renderTarget,
    const PixelShader& ps,
    const ConstantBuffer& cb,
    const TextureTable* tt,
    uint2 tileMin,
    uint2 tileMax)
{
    PROFILE_SCOPE("RasterizerScalar::RasterizeTriangle");

    float minX = std::min({v0.Position.x, v1.Position.x, v2.Position.x});
    float maxX = std::max({v0.Position.x, v1.Position.x, v2.Position.x});
    float minY = std::min({v0.Position.y, v1.Position.y, v2.Position.y});
    float maxY = std::max({v0.Position.y, v1.Position.y, v2.Position.y});

    uint iMinX = uint(std::max(double(tileMin.x), std::floor(minX)));
    uint iMaxX = uint(std::min(double(tileMax.x), std::ceil(maxX)));
    uint iMinY = uint(std::max(double(tileMin.y), std::floor(minY)));
    uint iMaxY = uint(std::min(double(tileMax.y), std::ceil(maxY)));

    if (iMinX > iMaxX || iMinY > iMaxY) UNLIKELY
        return;

    float area2 = RasterizerCommon::EdgeFunction(v0.Position, v1.Position, v2.Position);
    if (state.cullMode == CullMode::Back && area2 < 0) return;
    if (state.cullMode == CullMode::Front && area2 > 0) return;
    if (std::abs(area2) < 1e-6f) UNLIKELY return;

    uint width = renderTarget.Width();

    for (uint y = iMinY; y <= iMaxY; ++y)
    {
        for (uint x = iMinX; x <= iMaxX; ++x)
        {
            float2 p((float)x + 0.5f, (float)y + 0.5f);

            float f0 = RasterizerCommon::EdgeFunction(v1.Position, v2.Position, p);
            float f1 = RasterizerCommon::EdgeFunction(v2.Position, v0.Position, p);
            float f2 = RasterizerCommon::EdgeFunction(v0.Position, v1.Position, p);

            if (f0 * area2 < 0 || f1 * area2 < 0 || f2 * area2 < 0)
                continue;

            float a = f0 / area2;
            float b = f1 / area2;
            float c = f2 / area2;

            VertexOutput frag = RasterizerCommon::Trilerp(v0, v1, v2, a, b, c);
            uint idx = y * width + x;

            bool depthPass = false;
            switch (state.depthFunc)
            {
                // ... cases ...
            }

            if (depthPass)
            {
                depthBuffer.At(idx) = frag.Position.z;
                float4 finalColor = ps(frag, cb, *tt);
                renderTarget.SetPixel(uint2(x, y), finalColor);
            }
        }
    }
}

SOFTX_END
```

This document may be extended as the project evolves. Consistency is key — discuss any proposed changes with the team and update this file accordingly.
