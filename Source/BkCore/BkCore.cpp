#include "BkCore.h"

#include "BkString.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

namespace Bk
{
	bool AssertError(const char* expr, const char* file, int32 line, const char* format, ...)
	{
		TStringBuffer<1024> errorMessage;
		errorMessage.Appendf("ASSERTION FAILED: %s [%s:%d]\n", expr, file, line);

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

	DateTime GetUtcTime()
	{
		struct timeval time = {};
		gettimeofday(&time, nullptr);

		struct tm utcTime = {};
		gmtime_r(&time.tv_sec, &utcTime);

		DateTime result = {};
		result.year = static_cast<uint16>(utcTime.tm_year + 1900);
		result.month = static_cast<uint8>(utcTime.tm_mon + 1);
		result.weekday = static_cast<uint8>(utcTime.tm_wday);
		result.day = static_cast<uint8>(utcTime.tm_mday);
		result.hour = static_cast<uint8>(utcTime.tm_hour);
		result.minute = static_cast<uint8>(utcTime.tm_min);
		result.second = static_cast<uint8>(utcTime.tm_sec);
		result.millisecond = static_cast<uint16>(time.tv_usec / 1000);

		return result;
	}

	uint64 GetUnixTime()
	{
		time_t result = time(nullptr);
		return static_cast<uint64>(result);
	}
}
