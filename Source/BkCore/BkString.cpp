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

	bool String::Equals(String other, bool ignoreCase) const
	{
		if (length != other.length)
		{
			return false;
		}

		if (!ignoreCase)
		{
			return MemoryCompare(data, other.data, length) == 0;
		}

		for (size_t idx = 0; idx < length; ++idx)
		{
			if (ToUpper(data[idx]) != ToUpper(other.data[idx]))
			{
				return false;
			}
		}

		return true;
	}

	bool String::Contains(char search, bool ignoreCase) const
	{
		return Find(search, ignoreCase) != SIZE_MAX;
	}

	bool String::Contains(String search, bool ignoreCase) const
	{
		return Find(search, ignoreCase) != SIZE_MAX;
	}

	size_t String::Find(char search, bool ignoreCase) const
	{
		const char* start = data;
		const char* end = data + length;

		if (ignoreCase)
		{
			char searchUpper = ToUpper(search);
			for (const char* c = start; c < end; ++c)
			{
				if (ToUpper(*c) == searchUpper)
				{
					return static_cast<size_t>(c - start);
				}
			}
		}
		else
		{
			for (const char* c = start; c < end; ++c)
			{
				if (*c == search)
				{
					return static_cast<size_t>(c - start);
				}
			}
		}

		return SIZE_MAX;
	}

	size_t String::Find(String search, bool ignoreCase) const
	{
		if (search.length == 1)
		{
			return Find(search.data[0], ignoreCase);
		}
		else if (search.length > 1 && search.length <= length)
		{
			String slice(data, search.length);
			for (size_t idx = 0; idx <= length - search.length; ++idx)
			{
				if (slice.Equals(search, ignoreCase))
				{
					return idx;
				}

				slice.data += 1;
			}
		}

		return SIZE_MAX;
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

	StringBuffer::operator String() const
	{
		return String(data, length);
	}
}
