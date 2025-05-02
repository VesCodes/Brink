#pragma once

#include <stddef.h>
#include <stdint.h>

#define BK_ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define BK_ABS(x) ((x) < 0 ? -(x) : (x))
#define BK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BK_CLAMP(x, min, max) (((x) > (max)) ? (max) : ((x) < (min)) ? (min) : (x))

#define BK_ASSERT(expr) BK_ASSERTF(expr, "")
#define BK_ASSERTF(expr, format, ...) \
	do { \
		BK_CHECK_FORMAT(format, ##__VA_ARGS__); \
		if (!(expr) && Bk::AssertError(#expr, __FILE__, __LINE__, format, ##__VA_ARGS__)) { BK_BREAK(); } \
	} while(0)

#define BK_BREAK() emscripten_debugger()
extern "C" void emscripten_debugger(void);

#define BK_CHECK_FORMAT(format, ...) ((void)sizeof(printf(format, ##__VA_ARGS__)))
extern "C" int printf(const char*, ...);

namespace Bk
{
	using int8 = int8_t;
	using int16 = int16_t;
	using int32 = int32_t;
	using int64 = int64_t;

	using uint8 = uint8_t;
	using uint16 = uint16_t;
	using uint32 = uint32_t;
	using uint64 = uint64_t;

	bool AssertError(const char* expression, const char* file, int32 line, const char* format, ...);
	[[noreturn]] void FatalError(int32 exitCode, const char* format, ...);
}
