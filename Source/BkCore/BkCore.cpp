#include "BkCore.h"

#include "BkString.h"

#include <stdio.h>
#include <stdlib.h>

#if defined(BK_PLATFORM_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

namespace Bk
{
	bool AssertError(const char* expression, const char* file, int32 line, const char* format, ...)
	{
		TStringBuffer<1024> errorMessage;
		errorMessage.Appendf("ASSERTION FAILED: %s [%s:%d]\n", expression, file, line);

		if (format && format[0] != '\0')
		{
			va_list args;
			va_start(args, format);

			if (errorMessage.Appendv(format, args))
			{
				errorMessage.Append('\n');
			}

			va_end(args);
		}

		fwrite(errorMessage.data, sizeof(char), errorMessage.length, stderr);
		fflush(stderr);

		return true;
	}

	void FatalError(int32 exitCode, const char* format, ...)
	{
		TStringBuffer<1024> errorMessage;
		errorMessage.Appendf("FATAL ERROR: %d\n", exitCode);

		if (format && format[0] != '\0')
		{
			va_list args;
			va_start(args, format);

			if (errorMessage.Appendv(format, args))
			{
				errorMessage.Append('\n');
			}

			va_end(args);
		}

		fwrite(errorMessage.data, sizeof(char), errorMessage.length, stderr);
		fflush(stderr);

		exit(exitCode);
	}

	uint64 GetCpuTicks()
	{
#if defined(BK_PLATFORM_WINDOWS)
		LARGE_INTEGER counter;
		QueryPerformanceCounter(&counter);

		return static_cast<uint64>(counter.QuadPart);
#else
		timespec ts = {};
		clock_gettime(CLOCK_MONOTONIC, &ts);

		return static_cast<uint64>(ts.tv_sec * 1'000'000'000ll + ts.tv_nsec);
#endif
	}

	uint64 GetCpuFrequency()
	{
#if defined(BK_PLATFORM_WINDOWS)
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);

		return static_cast<uint64>(frequency.QuadPart);
#else
		return 1'000'000'000ull;
#endif
	}

	uint64 GetTimeMs()
	{
		uint64 period = GetCpuFrequency();
		return (GetCpuTicks() * 1'000) / period;
	}

	double GetTimeSec()
	{
		double period = 1.0 / static_cast<double>(GetCpuFrequency());
		return static_cast<double>(GetCpuTicks()) * period;
	}

	DateTime GetUtcTime()
	{
		DateTime result = {};

#if defined(BK_PLATFORM_WINDOWS)
		SYSTEMTIME systemTime;
		GetSystemTime(&systemTime);

		result.year = systemTime.wYear;
		result.month = static_cast<uint8>(systemTime.wMonth);
		result.weekday = static_cast<uint8>(systemTime.wDayOfWeek);
		result.day = static_cast<uint8>(systemTime.wDay);
		result.hour = static_cast<uint8>(systemTime.wHour);
		result.minute = static_cast<uint8>(systemTime.wMinute);
		result.second = static_cast<uint8>(systemTime.wSecond);
		result.millisecond = systemTime.wMilliseconds;
#else
		timeval time = {};
		gettimeofday(&time, nullptr);

		tm utcTime = {};
		gmtime_r(&time.tv_sec, &utcTime);

		result.year = static_cast<uint16>(utcTime.tm_year + 1900);
		result.month = static_cast<uint8>(utcTime.tm_mon + 1);
		result.weekday = static_cast<uint8>(utcTime.tm_wday);
		result.day = static_cast<uint8>(utcTime.tm_mday);
		result.hour = static_cast<uint8>(utcTime.tm_hour);
		result.minute = static_cast<uint8>(utcTime.tm_min);
		result.second = static_cast<uint8>(utcTime.tm_sec);
		result.millisecond = static_cast<uint16>(time.tv_usec / 1000);
#endif

		return result;
	}

	uint64 GetUnixTime()
	{
#if defined(BK_PLATFORM_WINDOWS)
		FILETIME fileTime = {};
		GetSystemTimeAsFileTime(&fileTime);

		constexpr uint64 epochOffset = 0x019DB1DED53E8000ull;
		constexpr uint64 period = 10'000'000ull; // 1sec in 100ns intervals

		return ((static_cast<uint64>(fileTime.dwHighDateTime) << 32 | fileTime.dwLowDateTime) - epochOffset) / period;
#else
		time_t result = time(nullptr);
		return static_cast<uint64>(result);
#endif
	}
}
