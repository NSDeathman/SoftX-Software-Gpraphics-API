SoftX C++ Coding Standard
This document defines the coding style and conventions for the SoftX project.
The primary inspirations are:

Unreal Engine coding style (brace placement, naming, namespace macros).

C++ Standard Library best practices (clarity, no abbreviations, modern language features).

All new code must conform to these guidelines to ensure consistency and readability across the codebase.

1. Naming
1.1. General Principles
Full, meaningful names without abbreviations.
Bad: btn, calc, tmp
Good: button, calculateSum, temporaryBuffer

English is used for all identifiers.

1.2. Namespaces
Namespace names use PascalCase. Use macros to explicitly mark the beginning and end of a namespace:

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

1.3. Classes, Structures, Enumerations
PascalCase without prefixes (Unreal‑style prefixes like F, U, etc., may be added if explicitly agreed upon).

Enumerations: type name in PascalCase, enumerators in PascalCase.

```cpp
class Renderer { ... };
struct VertexData { ... };
enum class BlendMode { Normal, Additive, Multiply };
```

1.4. Methods and Functions
PascalCase for all methods (including static and global functions).

Use verb or verb phrases that describe the action.

```cpp
void InitializeEngine();
int CalculateFrameRate() const;
void SetViewportSize(int width, int height);
```

1.5. Class/Struct Fields
PascalCase without prefixes (e.g., no m_ or trailing underscore unless explicitly required).

```cpp
class Player {
    float Health;
    int Score;
    std::string PlayerName;
};
```

1.6. Local Variables and Function Parameters
camelCase (starts with a lowercase letter).

```cpp
void ProcessInput(float deltaTime) {
    float movementSpeed = 10.0f;
    int frameCount = 0;
}
```

1.7. Macros and Compile-Time Constants
UPPER_SNAKE_CASE.

```cpp
#define SOFTX_API
#define MAX_BUFFER_SIZE 4096
constexpr int DEFAULT_PORT = 8080;
```

2. Formatting
2.1. Braces
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

2.2. Indentation
Use 4 spaces (no tabs).

Indent nested namespaces and classes.

2.3. Spaces
One space between a keyword and the opening parenthesis:
if (condition), for (int i = 0; i < count; ++i)

No spaces inside parentheses: (x + y) * z, func(a, b).

Binary operators are surrounded by spaces: a + b, x == y.

2.4. Line Length
Recommended maximum line length is 120 characters.

When breaking function arguments, place each new argument on a new line, indented.

```cpp
void LongFunctionName(int parameter1,
                      int parameter2,
                      float parameter3);
```
                      
3. Comments
3.1. Language
All comments must be written in English.

3.2. Documentation Comments
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

3.3. Inline Comments
Use // for single‑line comments.

Comment complex or non‑obvious code, but avoid stating the obvious.

```cpp
// Switch state only after input validation.
if (IsDataValid(input)) {
    SwitchState(input);
}
```

4. Macros and Preprocessor Directives
The macros SOFTX_BEGIN and SOFTX_END are used strictly to delimit the namespace content.

Define macros in header files with proper include guards (#pragma once is preferred).

Use SOFTX_API for symbol export/import, defined according to platform/compiler.

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

5. Code Organization
5.1. Header Files
Header file name should match the class/module name: Renderer.h, MathUtils.h.

Each header must start with #pragma once.

Include only what is necessary; prefer forward declarations.

5.2. Implementation Files (.cpp)
Include order:

Corresponding header (e.g., Renderer.cpp starts with #include "Renderer.h").
Other project headers.
External library headers.
Standard library headers.
Separate each group with a blank line.
5.3. Namespaces
All code must reside inside SOFTX_BEGIN / SOFTX_END (i.e., within namespace SoftX).

For nested namespaces use similar macros or explicit syntax.

6. Encoding and Character Set
All source files must be saved with UTF-8 encoding.

Stick to regular Unicode characters (primarily ASCII and characters from the Basic Multilingual Plane). Avoid invisible or special Unicode characters (e.g., non‑breaking spaces, directional markers, control characters) that could cause encoding or tooling issues.

This ensures consistent handling across different platforms and editors.

7. Additional Recommendations
7.1. Use of const
Mark methods that do not modify the object as const.

Use const for variables that should not change after initialization.

Prefer const T& for parameters instead of copying unless modification is needed.

7.2. Initialization
Use uniform initialization ({}) to prevent narrowing conversions.

```cpp
int value{42};
std::vector<int> numbers{1, 2, 3};
```

7.3. Error Handling
Exceptions are used only in critical cases (team agreement required).

For code that must not throw, use error codes or std::optional.

7.4. Modern C++
The project targets C++17 (or later). Prefer standard library facilities over manual resource management.

Use std::unique_ptr, std::shared_ptr for ownership.

8. Code Example
```cpp
// Player.h
#pragma once

#include "CoreTypes.h"
#include <string>

SOFTX_BEGIN

class Player
{
public:
    Player() = default;
    explicit Player(std::string&& name);

    std::string GetName() const { return PlayerName; }
    void SetName(const std::string& name) { PlayerName = name; }

    float GetHealth() const { return Health; }
    void TakeDamage(float amount);

private:
    std::string PlayerName;
    float Health{100.0f};
};

SOFTX_END
```

```cpp
// Player.cpp
#include "Player.h"

SOFTX_BEGIN

Player::Player(std::string&& name)
    : PlayerName(std::move(name))
{
}

void Player::TakeDamage(float amount)
{
    if (amount > 0.0f)
    {
        Health -= amount;
        if (Health < 0.0f)
            Health = 0.0f;
    }
}

SOFTX_END
```

This document may be extended as the project evolves. Consistency is key — discuss any proposed changes with the team and update this file accordingly.
