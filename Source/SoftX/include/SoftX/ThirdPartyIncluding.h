#pragma once

// Math lib
#include "../AfterMath/include/AfterMath.h"
using namespace AfterMath;

// Your custom profiler
#ifdef ENABLE_PROFILER
#include "../../../Optick/Include/optick.h"
#define PROFILE_SCOPE(x) OPTICK_EVENT(x)
#define PROFILE_THREAD(x) OPTICK_THREAD(x)
#else
#define PROFILE_SCOPE(x)
#define PROFILE_THREAD(x)
#endif