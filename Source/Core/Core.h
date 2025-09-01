#pragma once

#include <stddef.h>
#include <stdint.h>

#ifndef BK_PLATFORM_WINDOWS
#if defined(_WIN32)
#define BK_PLATFORM_WINDOWS 1
#else
#define BK_PLATFORM_WINDOWS 0
#endif
#endif

#ifndef BK_PLATFORM_MACOS
#if defined(__APPLE__)
#define BK_PLATFORM_MACOS 1
#else
#define BK_PLATFORM_MACOS 0
#endif
#endif

#ifndef BK_PLATFORM_EMSCRIPTEN
#if defined(__EMSCRIPTEN__)
#define BK_PLATFORM_EMSCRIPTEN 1
#else
#define BK_PLATFORM_EMSCRIPTEN 0
#endif
#endif

#define BK_ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

#define BK_ASSERT(expr) BK_ASSERTF(expr, "")
#define BK_ASSERTF(expr, format, ...) \
	do { \
		BK_CHECK_FORMAT(format, ## __VA_ARGS__); \
		if (!(expr) && Bk::AssertError(#expr, __FILE__, __LINE__, format, ## __VA_ARGS__)) { BK_BREAK(); } \
	} while(0)

#if BK_PLATFORM_EMSCRIPTEN
#define BK_BREAK() emscripten_debugger()
extern "C" void emscripten_debugger(void);
#else
#define BK_BREAK() __builtin_debugtrap()
#endif

#define BK_CHECK_FORMAT(format, ...) ((void)sizeof(printf(format, ## __VA_ARGS__)))
extern "C" int printf(const char*, ...);

#define BK_ENUM_FLAGS(EnumType) \
	constexpr EnumType& operator&=(EnumType& a, EnumType b) { return a = (EnumType)((__underlying_type(EnumType))a & (__underlying_type(EnumType))b); } \
	constexpr EnumType& operator|=(EnumType& a, EnumType b) { return a = (EnumType)((__underlying_type(EnumType))a | (__underlying_type(EnumType))b); } \
	constexpr EnumType& operator^=(EnumType& a, EnumType b) { return a = (EnumType)((__underlying_type(EnumType))a ^ (__underlying_type(EnumType))b); } \
	constexpr EnumType operator&(EnumType a, EnumType b) { return (EnumType)((__underlying_type(EnumType))a & (__underlying_type(EnumType))b); } \
	constexpr EnumType operator|(EnumType a, EnumType b) { return (EnumType)((__underlying_type(EnumType))a | (__underlying_type(EnumType))b); } \
	constexpr EnumType operator^(EnumType a, EnumType b) { return (EnumType)((__underlying_type(EnumType))a ^ (__underlying_type(EnumType))b); } \
	constexpr EnumType operator~(EnumType e) { return (EnumType)(~(__underlying_type(EnumType))e); }

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

namespace Bk
{
	template<typename T>
	constexpr T Abs(T value)
	{
		return value < 0 ? -value : value;
	}

	template<typename T, typename... ArgTypes>
	constexpr T Min(T value, ArgTypes... args)
	{
		((value = args < value ? args : value), ...);
		return value;
	}

	template<typename T, typename... ArgTypes>
	constexpr T Max(T value, ArgTypes... args)
	{
		((value = args > value ? args : value), ...);
		return value;
	}

	template<typename T>
	constexpr T Clamp(T value, T min, T max)
	{
		return Max(min, Min(value, max));
	}

	template<typename EnumType>
	bool EnumHasAllFlags(EnumType value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		return (static_cast<UnderlyingType>(value) & static_cast<UnderlyingType>(flags)) == static_cast<UnderlyingType>(flags);
	}

	template<typename EnumType>
	bool EnumHasAnyFlags(EnumType value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		return (static_cast<UnderlyingType>(value) & static_cast<UnderlyingType>(flags)) != 0;
	}

	template<typename EnumType>
	void EnumAddFlags(EnumType& value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		value = static_cast<EnumType>(static_cast<UnderlyingType>(value) | static_cast<UnderlyingType>(flags));
	}

	template<typename EnumType>
	void EnumRemoveFlags(EnumType& value, EnumType flags)
	{
		using UnderlyingType = __underlying_type(EnumType);
		value = static_cast<EnumType>(static_cast<UnderlyingType>(value) & ~static_cast<UnderlyingType>(flags));
	}

	bool AssertError(const char* expression, const char* file, int32 line, const char* format, ...);
	[[noreturn]] void FatalError(int32 exitCode, const char* format, ...);

	struct DateTime
	{
		uint16 year;
		uint8 month;        // [1, 12]
		uint8 weekday;      // [0, 6]
		uint8 day;          // [1, 31]
		uint8 hour;         // [0, 23]
		uint8 minute;       // [0, 59]
		uint8 second;       // [0, 59]
		uint16 millisecond; // [0, 999]
	};

	uint64 GetPackedTimeFromDateTime(DateTime time);
	DateTime GetDateTimeFromPackedTime(uint64 time);

	uint64 GetUnixTimeFromDateTime(DateTime time);
}
