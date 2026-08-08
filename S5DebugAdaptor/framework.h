#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files
#include <windows.h>

#ifdef USE_CLANG_NAKED
#define NAKED_DECL __attribute((naked))
#define NAKED_DEF
#define NAKED __attribute((naked))
#else
#define NAKED_DEF __declspec(naked)
#define NAKED_DECL
#define NAKED __declspec(naked)
#endif
