#include "BkString.h"

#include <stdio.h>

namespace Bk
{
	int32 StringPrintf(char* dst, size_t dstLength, const char* format, ...)
	{
		va_list args;
		va_start(args, format);

		const int32 result = StringPrintv(dst, dstLength, format, args);
		va_end(args);

		return result;
	}

	int32 StringPrintv(char* dst, size_t dstLength, const char* format, va_list args)
	{
		return vsnprintf(dst, dstLength, format, args);
	}

	String String::Slice(size_t offset, size_t count) const
	{
		BK_ASSERT(offset >= 0 && offset < length);
		return String(data + offset, BK_MIN(count, length - offset));
	}

	char String::operator[](size_t index) const
	{
		BK_ASSERT(index >= 0 && index < length);
		return data[index];
	}

	const char* String::begin() const
	{
		return data;
	}

	const char* String::end() const
	{
		return data + length;
	}
}
