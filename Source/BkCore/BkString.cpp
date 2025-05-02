#include "BkString.h"
#include "BkMemory.h"

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

	StringBuffer::StringBuffer(char* buffer, size_t bufferSize)
		: data(buffer), length(0), capacity(bufferSize)
	{
		if (capacity > 0)
		{
			data[0] = '\0';
		}
	}

	bool StringBuffer::Append(char c)
	{
		if (length + 1 >= capacity)
		{
			return false;
		}

		data[length] = c;

		length += 1;
		data[length] = '\0';

		return true;
	}

	bool StringBuffer::Append(String string)
	{
		if (length + string.length >= capacity)
		{
			return false;
		}

		MemoryCopy(data + length, string.data, string.length);

		length += string.length;
		data[length] = '\0';

		return true;
	}

	bool StringBuffer::Appendf(const char* format, ...)
	{
		if (length >= capacity)
		{
			return false;
		}

		va_list args;
		va_start(args, format);
		bool result = Appendv(format, args);
		va_end(args);

		return result;
	}

	bool StringBuffer::Appendv(const char* format, va_list args)
	{
		const size_t availableSize = capacity - length;

		const int32 result = StringPrintv(data + length, availableSize, format, args);
		if (result >= 0 && static_cast<size_t>(result) < availableSize)
		{
			length += static_cast<size_t>(result);
			return true;
		}

		return false;
	}

	void StringBuffer::Reset()
	{
		if (capacity > 0)
		{
			data[0] = '\0';
		}

		length = 0;
	}
}
