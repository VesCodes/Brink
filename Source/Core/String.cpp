#include "String.h"

#include "Memory.h"

#include <float.h>
#include <math.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>

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
		return stbsp_vsnprintf(dst, dstLength, format, args);
	}

	String String::Slice(size_t offset, size_t count) const
	{
		BK_ASSERT(offset <= length);
		return String(data + offset, BK_MIN(count, length - offset));
	}

	String String::Range(size_t start, size_t end) const
	{
		BK_ASSERT(start <= length && start <= end);
		return String(data + start, BK_MIN(end - start, length - start));
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

		if (search.length > 1 && search.length <= length)
		{
			String slice(data, search.length);
			for (size_t i = 0; i <= length - search.length; ++i)
			{
				if (slice.Equals(search, ignoreCase))
				{
					return static_cast<size_t>(slice.data - data);
				}

				slice.data += 1;
			}
		}

		return SIZE_MAX;
	}

	size_t String::FindLast(char search, bool ignoreCase) const
	{
		if (length == 0)
		{
			return SIZE_MAX;
		}

		const char* start = data;
		const char* end = data + length;

		if (ignoreCase)
		{
			char searchUpper = ToUpper(search);
			for (const char* c = end - 1; c >= start; --c)
			{
				if (ToUpper(*c) == searchUpper)
				{
					return static_cast<size_t>(c - start);
				}
			}
		}
		else
		{
			for (const char* c = end - 1; c >= start; --c)
			{
				if (*c == search)
				{
					return static_cast<size_t>(c - start);
				}
			}
		}

		return SIZE_MAX;
	}

	size_t String::FindLast(String search, bool ignoreCase) const
	{
		if (search.length == 1)
		{
			return FindLast(search.data[0], ignoreCase);
		}

		if (search.length > 1 && search.length <= length)
		{
			String slice(data + length - search.length, search.length);
			for (size_t i = 0; i <= length - search.length; ++i)
			{
				if (slice.Equals(search, ignoreCase))
				{
					return static_cast<size_t>(slice.data - data);
				}

				slice.data -= 1;
			}
		}

		return SIZE_MAX;
	}

	bool String::Parse(bool& value) const
	{
		if (Equals("true", true) || Equals("1"))
		{
			value = true;
			return true;
		}

		if (Equals("false", true) || Equals("0"))
		{
			value = false;
			return true;
		}

		return false;
	}

	bool String::Parse(int64& value) const
	{
		if (length == 0)
		{
			return false;
		}

		const char* c = data;
		const char* end = data + length;

		bool negative = false;
		if (*c == '-')
		{
			negative = true;
			c += 1;
		}
		else if (*c == '+')
		{
			c += 1;
		}

		const uint64 limit = negative ? static_cast<uint64>(INT64_MAX) + 1 : static_cast<uint64>(INT64_MAX);
		const uint64 maxDiv10 = limit / 10;
		const uint64 maxMod10 = limit % 10;

		uint64 result = 0;
		bool hasDigits = false;

		for (; c != end && IsDigit(*c); ++c)
		{
			uint64 digit = static_cast<uint64>(*c - '0');
			if (result > maxDiv10 || (result == maxDiv10 && digit > maxMod10))
			{
				return false;
			}

			result = result * 10 + digit;
			hasDigits = true;
		}

		if (!hasDigits || c != end)
		{
			return false;
		}

		value = negative ? -static_cast<int64>(result) : static_cast<int64>(result);

		return true;
	}

	bool String::Parse(int32& value) const
	{
		int64 result;
		if (Parse(result) && result >= INT32_MIN && result <= INT32_MAX)
		{
			value = static_cast<int32>(result);
			return true;
		}

		return false;
	}

	bool String::Parse(uint64& value) const
	{
		if (length == 0)
		{
			return false;
		}

		const char* c = data;
		const char* end = data + length;

		if (*c == '-')
		{
			return false;
		}
		if (*c == '+')
		{
			c += 1;
		}

		const uint64 maxDiv10 = UINT64_MAX / 10;
		const uint64 maxMod10 = UINT64_MAX % 10;

		uint64 result = 0;
		bool hasDigits = false;

		for (; c != end && IsDigit(*c); ++c)
		{
			uint64 digit = static_cast<uint64>(*c - '0');
			if (result > maxDiv10 || (result == maxDiv10 && digit > maxMod10))
			{
				return false;
			}

			result = result * 10 + digit;
			hasDigits = true;
		}

		if (!hasDigits || c != end)
		{
			return false;
		}

		value = result;

		return true;
	}

	bool String::Parse(uint32& value) const
	{
		uint64 result;
		if (Parse(result) && result <= UINT32_MAX)
		{
			value = static_cast<uint32>(result);
			return true;
		}

		return false;
	}

	bool String::Parse(double& value) const
	{
		if (length == 0)
		{
			return false;
		}

		const char* c = data;
		const char* end = data + length;

		double sign = 1.0;
		if (*c == '-')
		{
			sign = -1.0;
			c += 1;
		}
		else if (*c == '+')
		{
			c += 1;
		}

		double result = 0.0;
		bool hasMantissa = false;

		for (; c != end && IsDigit(*c); ++c)
		{
			result = result * 10.0 + (*c - '0');
			hasMantissa = true;
		}

		if (c != end && *c == '.')
		{
			c += 1;

			double power = 0.1;
			for (; c != end && IsDigit(*c); ++c)
			{
				result += (*c - '0') * power;
				power *= 0.1;

				hasMantissa = true;
			}
		}

		if (!hasMantissa)
		{
			return false;
		}

		if (c != end && (*c == 'e' || *c == 'E'))
		{
			c += 1;

			double exponentSign = 1.0;
			if (c != end)
			{
				if (*c == '-')
				{
					exponentSign = -1.0;
					c += 1;
				}
				else if (*c == '+')
				{
					c += 1;
				}
			}

			double exponent = 0.0;
			bool hasExponent = false;

			for (; c != end && IsDigit(*c); ++c)
			{
				exponent = exponent * 10.0 + (*c - '0');
				hasExponent = true;
			}

			if (!hasExponent)
			{
				return false;
			}

			result *= pow(10.0, exponent * exponentSign);
		}

		if (!isfinite(result) || c != end)
		{
			return false;
		}

		value = result * sign;

		return true;
	}

	bool String::Parse(float& value) const
	{
		double result;
		if (Parse(result) && result >= -FLT_MAX && result <= FLT_MAX)
		{
			value = static_cast<float>(result);
			return true;
		}

		return false;
	}

	char String::operator[](size_t index) const
	{
		BK_ASSERT(index < length);
		return data[index];
	}

	bool String::operator==(String other) const
	{
		return length == other.length && MemoryCompare(data, other.data, length) == 0;
	}

	bool String::operator!=(String other) const
	{
		return length != other.length || MemoryCompare(data, other.data, length) != 0;
	}

	const char* String::begin() const
	{
		return data;
	}

	const char* String::end() const
	{
		return data + length;
	}

	StringBuilder::StringBuilder(char* buffer, size_t bufferSize)
		: arena(nullptr)
	{
		chunk.previous = nullptr;
		chunk.buffer = buffer;
		chunk.capacity = bufferSize;
		chunk.length = 0;
		length = 0;
	}

	StringBuilder::StringBuilder(Arena& arena)
		: arena(&arena)
	{
		chunk.previous = nullptr;
		chunk.buffer = nullptr;
		chunk.capacity = 0;
		chunk.length = 0;
		length = 0;
	}

	bool StringBuilder::Append(char c)
	{
		if (chunk.length + 1 > chunk.capacity)
		{
			if (!Expand(1))
			{
				return false;
			}
		}

		chunk.buffer[chunk.length] = c;
		chunk.length += 1;
		length += 1;

		return true;
	}

	bool StringBuilder::Append(String string)
	{
		while (string.length > 0)
		{
			size_t chunkRemaining = chunk.capacity - chunk.length;
			if (chunkRemaining == 0)
			{
				if (!Expand(string.length))
				{
					return false;
				}

				chunkRemaining = chunk.capacity;
			}

			size_t sliceLength = BK_MIN(string.length, chunkRemaining);

			MemoryCopy(chunk.buffer + chunk.length, string.data, sliceLength);
			chunk.length += sliceLength;
			length += sliceLength;

			string = string.Slice(sliceLength);
		}

		return true;
	}

	bool StringBuilder::Appendf(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		bool result = Appendv(format, args);
		va_end(args);

		return result;
	}

	bool StringBuilder::Appendv(const char* format, va_list args)
	{
		char buffer[STB_SPRINTF_MIN];

		int32 result = stbsp_vsprintfcb(
			[](const char* buffer, void* context, int32 length)
			{
				StringBuilder* builder = static_cast<StringBuilder*>(context);
				bool result = builder->Append(String(buffer, length));

				return result ? const_cast<char*>(buffer) : nullptr;
			},
			this, buffer, format, args);

		return (result >= 0 && result < sizeof(buffer));
	}

	bool StringBuilder::Expand(size_t requiredCapacity)
	{
		if (!arena)
		{
			return false;
		}

		Chunk* chainedChunk = arena->Push<Chunk>();
		chainedChunk->previous = chunk.previous;
		chainedChunk->buffer = chunk.buffer;
		chainedChunk->capacity = chunk.capacity;
		chainedChunk->length = chunk.length;

		chunk.previous = chainedChunk;
		chunk.capacity = BK_MAX(BK_MAX(requiredCapacity, 64), BK_MIN(length, 8096));
		chunk.buffer = arena->Push<char>(chunk.capacity);
		chunk.length = 0;

		return true;
	}

	String StringBuilder::ToString(Arena& arena, bool nullTerminate) const
	{
		TSpan<char> buffer = arena.Push<char>(nullTerminate ? length + 1 : length);

		if (nullTerminate)
		{
			buffer[length] = '\0';
		}

		char* bufferPtr = buffer.data + length;
		for (const Chunk* chainedChunk = &chunk; chainedChunk; chainedChunk = chainedChunk->previous)
		{
			bufferPtr -= chainedChunk->length;
			MemoryCopy(bufferPtr, chainedChunk->buffer, chainedChunk->length);
		}

		// Ensure the entire buffer has been filled
		BK_ASSERT(buffer.data == bufferPtr);

		return String(buffer.data, buffer.length);
	}
}
