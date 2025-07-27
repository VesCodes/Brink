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
}
