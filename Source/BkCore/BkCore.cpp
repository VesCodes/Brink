#include "BkCore.h"

#include <stdlib.h>

namespace Bk
{
	bool AssertError(const char* expr, const char* file, int32 line, const char* format, ...)
	{
		// #TODO: Error message
		return true;
	}

	void FatalError(int32 exitCode, const char* format, ...)
	{
		// #TODO: Error message
		exit(exitCode);
	}
}
