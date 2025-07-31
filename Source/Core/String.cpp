#include "String.h"

#include "Memory.h"

#include <float.h>
#include <math.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>

namespace Bk
{
	const String String::Empty = {};

	String String::Slice(size_t start, size_t count) const
	{
		BK_ASSERT(start <= length);
		return String(data + start, Min(count, length - start));
	}

	String String::Range(size_t start, size_t end) const
	{
		BK_ASSERT(start <= length && start <= end);
		return String(data + start, Min(end - start, length - start));
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

	bool String::StartsWith(String prefix, bool ignoreCase) const
	{
		if (prefix.length > length)
		{
			return false;
		}

		return Slice(0, prefix.length).Equals(prefix, ignoreCase);
	}

	bool String::EndsWith(String suffix, bool ignoreCase) const
	{
		if (suffix.length > length)
		{
			return false;
		}

		return Slice(length - suffix.length).Equals(suffix, ignoreCase);
	}

	String String::Trim() const
	{
		size_t start = 0;
		size_t end = length;

		while (start < length && IsSpace(data[start]))
		{
			start += 1;
		}

		while (end > start && IsSpace(data[end - 1]))
		{
			end -= 1;
		}

		return Range(start, end);
	}

	String String::TrimQuotes() const
	{
		size_t start = 0;
		size_t end = length;

		if (length > 0 && data[0] == '"')
		{
			start = 1;
		}

		if (length > 1 && data[length - 1] == '"')
		{
			end = length - 1;
		}

		return Range(start, end);
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

	bool ParseValue(String stream, bool& value)
	{
		if (stream.length == 0)
		{
			return false;
		}

		if (stream.Equals("true", true) || stream.Equals("1"))
		{
			value = true;
			return true;
		}

		if (stream.Equals("false", true) || stream.Equals("0"))
		{
			value = false;
			return true;
		}

		return false;
	}

	bool ParseValue(String stream, int64& value)
	{
		if (stream.length == 0)
		{
			return false;
		}

		const char* c = stream.data;
		const char* end = stream.data + stream.length;

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

	bool ParseValue(String stream, int32& value)
	{
		int64 result;
		if (ParseValue(stream, result) && result >= INT32_MIN && result <= INT32_MAX)
		{
			value = static_cast<int32>(result);
			return true;
		}

		return false;
	}

	bool ParseValue(String stream, uint64& value)
	{
		if (stream.length == 0)
		{
			return false;
		}

		const char* c = stream.data;
		const char* end = stream.data + stream.length;

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

	bool ParseValue(String stream, uint32& value)
	{
		uint64 result;
		if (ParseValue(stream, result) && result <= UINT32_MAX)
		{
			value = static_cast<uint32>(result);
			return true;
		}

		return false;
	}

	bool ParseValue(String stream, double& value)
	{
		if (stream.length == 0)
		{
			return false;
		}

		const char* c = stream.data;
		const char* end = stream.data + stream.length;

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

	bool ParseValue(String stream, float& value)
	{
		double result;
		if (ParseValue(stream, result) && result >= -FLT_MAX && result <= FLT_MAX)
		{
			value = static_cast<float>(result);
			return true;
		}

		return false;
	}

	bool ParseToken(String& stream, String& token)
	{
		size_t tokenStart = 0;
		while (tokenStart < stream.length && IsSpace(stream.data[tokenStart]))
		{
			tokenStart += 1;
		}

		stream = stream.Slice(tokenStart);
		if (stream.length == 0)
		{
			return false;
		}

		bool inQuotes = false;
		size_t tokenEnd = stream.length;

		for (size_t i = 0; i < stream.length; ++i)
		{
			if (stream.data[i] == '"' && (i == 0 || stream.data[i - 1] != '\\'))
			{
				inQuotes = !inQuotes;
			}
			else if (!inQuotes && IsSpace(stream.data[i]))
			{
				tokenEnd = i;
				break;
			}
		}

		token = stream.Slice(0, tokenEnd);
		stream = stream.Slice(tokenEnd);

		return true;
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

			size_t sliceLength = Min(string.length, chunkRemaining);

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
				bool result = builder->Append(String(buffer, static_cast<size_t>(length)));

				return result ? const_cast<char*>(buffer) : nullptr;
			},
			this, buffer, format, args);

		return (result >= 0 && static_cast<size_t>(result) < sizeof(buffer));
	}

	bool StringBuilder::AppendLine(String line)
	{
		if (!Append(line))
		{
			return false;
		}

		return Append('\n');
	}

	bool StringBuilder::AppendLinef(const char* format, ...)
	{
		va_list args;
		va_start(args, format);

		bool result = Appendv(format, args);
		va_end(args);

		return result && Append('\n');
	}

	bool StringBuilder::AppendPath(String path)
	{
		if (chunk.length > 0)
		{
			char lastChar = chunk.buffer[chunk.length - 1];
			if (lastChar != '/' && lastChar != '\\')
			{
				if (!Append('/'))
				{
					return false;
				}
			}
		}

		return Append(path);
	}

	bool StringBuilder::AppendPathf(const char* format, ...)
	{
		if (chunk.length > 0)
		{
			char lastChar = chunk.buffer[chunk.length - 1];
			if (lastChar != '/' && lastChar != '\\')
			{
				if (!Append('/'))
				{
					return false;
				}
			}
		}

		va_list args;
		va_start(args, format);

		bool result = Appendv(format, args);
		va_end(args);

		return result;
	}

	bool StringBuilder::Expand(size_t requiredCapacity)
	{
		for (Chunk* chainedChunk = &chunk; chainedChunk && chainedChunk->previous; chainedChunk = chainedChunk->previous)
		{
			Chunk* reusableChunk = chainedChunk->previous;
			if (reusableChunk->length == 0)
			{
				chainedChunk->previous = reusableChunk->previous;

				Chunk archivedChunk = chunk;

				chunk = *reusableChunk;
				chunk.previous = reusableChunk;

				*reusableChunk = archivedChunk;

				return true;
			}
		}

		if (!arena)
		{
			return false;
		}

		Chunk* archivedChunk = nullptr;
		if (chunk.capacity > 0)
		{
			archivedChunk = arena->Push<Chunk>();
			*archivedChunk = chunk;
		}

		constexpr size_t minCapacity = 64;
		constexpr size_t maxCapacityGrowth = 8096;

		chunk.previous = archivedChunk;
		chunk.capacity = Max(requiredCapacity, minCapacity, Min(length, maxCapacityGrowth));
		chunk.buffer = arena->Push<char>(chunk.capacity);
		chunk.length = 0;

		return true;
	}

	void StringBuilder::Reset()
	{
		for (Chunk* chainedChunk = &chunk; chainedChunk; chainedChunk = chainedChunk->previous)
		{
			chainedChunk->length = 0;
		}

		length = 0;
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

		return String(buffer.data, length);
	}

	String GetDirectoryName(String path)
	{
		size_t slashIdx = path.FindLast('/');
		if (slashIdx == SIZE_MAX)
		{
			slashIdx = path.FindLast('\\');
		}

		return slashIdx != SIZE_MAX ? path.Slice(0, slashIdx) : String::Empty;
	}

	String GetFileName(String path)
	{
		size_t slashIdx = path.FindLast('/');
		if (slashIdx == SIZE_MAX)
		{
			slashIdx = path.FindLast('\\');
		}

		return slashIdx != SIZE_MAX ? path.Slice(slashIdx + 1) : path;
	}

	String GetFileNameWithoutExtension(String path)
	{
		String fileName = GetFileName(path);
		size_t dotIdx = fileName.FindLast('.');

		return dotIdx != SIZE_MAX ? fileName.Slice(0, dotIdx) : fileName;
	}

	String GetExtension(String path)
	{
		String fileName = GetFileName(path);
		size_t dotIdx = fileName.FindLast('.');

		return dotIdx != SIZE_MAX ? fileName.Slice(dotIdx + 1) : String::Empty;
	}
}
