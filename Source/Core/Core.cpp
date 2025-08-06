#include "Core.h"

#include "String.h"

#include <stdio.h>
#include <stdlib.h>

namespace Bk
{
	bool AssertError(const char* expression, const char* file, int32 line, const char* format, ...)
	{
		TStringBuilder<1024> errorMessage;
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

		fwrite(errorMessage.buffer, sizeof(char), errorMessage.length, stderr);
		fflush(stderr);

		return true;
	}

	void FatalError(int32 exitCode, const char* format, ...)
	{
		TStringBuilder<1024> errorMessage;
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

		fwrite(errorMessage.buffer, sizeof(char), errorMessage.length, stderr);
		fflush(stderr);

		exit(exitCode);
	}

	uint64 GetPackedTimeFromDateTime(DateTime time)
	{
		uint64 result = time.millisecond;
		result |= static_cast<uint64>(time.second) << 10;
		result |= static_cast<uint64>(time.minute) << 16;
		result |= static_cast<uint64>(time.hour) << 22;
		result |= static_cast<uint64>(time.day) << 27;
		result |= static_cast<uint64>(time.month) << 32;
		result |= static_cast<uint64>(time.year) << 36;

		return result;
	}

	DateTime GetDateTimeFromPackedTime(uint64 time)
	{
		DateTime result = {};
		result.millisecond = static_cast<uint16>(time & 0x03FF);
		result.second = static_cast<uint8>(time >> 10 & 0x3F);
		result.minute = static_cast<uint8>(time >> 16 & 0x3F);
		result.hour = static_cast<uint8>(time >> 22 & 0x1F);
		result.day = static_cast<uint8>(time >> 27 & 0x1F);
		result.month = static_cast<uint8>(time >> 32 & 0x0F);
		result.year = static_cast<uint16>(time >> 36 & 0xFFFF);

		return result;
	}

	uint64 GetUnixTimeFromDateTime(DateTime time)
	{
		constexpr uint16 DaysToMonth[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };

		uint64 totalDays = (time.year - 1970) * 365ull;
		for (uint32 leapYear = 1972; leapYear < time.year; leapYear += 4)
		{
			if (leapYear % 100 != 0 || leapYear % 400 == 0)
			{
				totalDays += 1;
			}
		}

		totalDays += DaysToMonth[time.month - 1];
		if (time.month > 2 && time.year % 4 == 0 && (time.year % 100 != 0 || time.year % 400 == 0))
		{
			totalDays += 1;
		}

		totalDays += time.day - 1;

		return totalDays * 86400ull + time.hour * 3600ull + time.minute * 60ull + time.second;
	}
}
