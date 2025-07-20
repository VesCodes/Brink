#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#define BK_PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define BK_PLATFORM_MACOS
#elif defined(__EMSCRIPTEN__)
#define BK_PLATFORM_EMSCRIPTEN
#endif

#define BK_ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define BK_ABS(x) ((x) < 0 ? -(x) : (x))
#define BK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BK_CLAMP(x, min, max) (((x) > (max)) ? (max) : ((x) < (min)) ? (min) : (x))

#define BK_ASSERT(expr) BK_ASSERTF(expr, "")
#define BK_ASSERTF(expr, format, ...) \
	do { \
		BK_CHECK_FORMAT(format, ## __VA_ARGS__); \
		if (!(expr) && Bk::AssertError(#expr, __FILE__, __LINE__, format, ## __VA_ARGS__)) { BK_BREAK(); } \
	} while(0)

#if defined(BK_PLATFORM_EMSCRIPTEN)
#define BK_BREAK() emscripten_debugger()
extern "C" void emscripten_debugger(void);
#else
#define BK_BREAK() __builtin_debugtrap()
#endif

#define BK_CHECK_FORMAT(format, ...) ((void)sizeof(printf(format, ## __VA_ARGS__)))
extern "C" int printf(const char*, ...);

#define BK_ENUM_CLASS_FLAGS(EnumType) \
	constexpr EnumType& operator&=(EnumType& a, EnumType b) { return a = (EnumType)((__underlying_type(EnumType))a & (__underlying_type(EnumType))b); } \
	constexpr EnumType& operator|=(EnumType& a, EnumType b) { return a = (EnumType)((__underlying_type(EnumType))a | (__underlying_type(EnumType))b); } \
	constexpr EnumType& operator^=(EnumType& a, EnumType b) { return a = (EnumType)((__underlying_type(EnumType))a ^ (__underlying_type(EnumType))b); } \
	constexpr EnumType operator&(EnumType a, EnumType b) { return (EnumType)((__underlying_type(EnumType))a & (__underlying_type(EnumType))b); } \
	constexpr EnumType operator|(EnumType a, EnumType b) { return (EnumType)((__underlying_type(EnumType))a | (__underlying_type(EnumType))b); } \
	constexpr EnumType operator^(EnumType a, EnumType b) { return (EnumType)((__underlying_type(EnumType))a ^ (__underlying_type(EnumType))b); } \
	constexpr EnumType operator~(EnumType e) { return (EnumType)(~(__underlying_type(EnumType))e); }

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

	template<typename EnumType>
	bool EnumHasAllFlags(EnumType value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		return ((UnderlyingType)value & (UnderlyingType)flags) == (UnderlyingType)flags;
	}

	template<typename EnumType>
	bool EnumHasAnyFlags(EnumType value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		return ((UnderlyingType)value & (UnderlyingType)flags) != 0;
	}

	template<typename EnumType>
	void EnumAddFlags(EnumType& value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		value = (EnumType)((UnderlyingType)value | (UnderlyingType)flags);
	}

	template<typename EnumType>
	void EnumRemoveFlags(EnumType& value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		value = (EnumType)((UnderlyingType)value & ~(UnderlyingType)flags);
	}

	bool AssertError(const char* expression, const char* file, int32 line, const char* format, ...);
	[[noreturn]] void FatalError(int32 exitCode, const char* format, ...);

	struct DateTime
	{
		uint16 year;
		uint8 month;
		uint8 weekday;
		uint8 day;
		uint8 hour;
		uint8 minute;
		uint8 second;
		uint16 millisecond;
	};

	uint64 GetCpuTicks();
	uint64 GetCpuFrequency();

	uint64 GetTimeMs();
	double GetTimeSec();

	DateTime GetUtcTime();
	uint64 GetUnixTime();
}
