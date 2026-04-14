#include "String.h"

#include "Memory.h"

#include <float.h>
#include <math.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb_sprintf.h>

namespace Bk
{
	const String String::Empty = {};

	char String::operator[](size_t idx) const
	{
		BK_ASSERT(idx < length);
		return data[idx];
	}

	String Slice(String string, size_t start, size_t count)
	{
		BK_ASSERT(start <= string.length);
		return String(string.data + start, Min(count, string.length - start));
	}

	bool Equals(String string, String other, bool ignoreCase)
	{
		if (string.length != other.length)
		{
			return false;
		}

		if (!ignoreCase)
		{
			return CompareMemory(string.data, other.data, string.length) == 0;
		}

		for (size_t idx = 0; idx < string.length; ++idx)
		{
			if (ToUpper(string.data[idx]) != ToUpper(other.data[idx]))
			{
				return false;
			}
		}

		return true;
	}

	bool Contains(String string, char search, bool ignoreCase)
	{
		return Find(string, search, ignoreCase) != SIZE_MAX;
	}

	bool Contains(String string, String search, bool ignoreCase)
	{
		return Find(string, search, ignoreCase) != SIZE_MAX;
	}

	size_t Find(String string, char search, bool ignoreCase)
	{
		const char* start = string.data;
		const char* end = string.data + string.length;

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

	size_t Find(String string, String search, bool ignoreCase)
	{
		if (search.length == 1)
		{
			return Find(string, search.data[0], ignoreCase);
		}

		if (search.length > 1 && search.length <= string.length)
		{
			String slice(string.data, search.length);
			for (size_t i = 0; i <= string.length - search.length; ++i)
			{
				if (Equals(slice, search, ignoreCase))
				{
					return static_cast<size_t>(slice.data - string.data);
				}

				slice.data += 1;
			}
		}

		return SIZE_MAX;
	}

	size_t FindLast(String string, char search, bool ignoreCase)
	{
		if (string.length == 0)
		{
			return SIZE_MAX;
		}

		const char* start = string.data;
		const char* end = string.data + string.length;

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

	size_t FindLast(String string, String search, bool ignoreCase)
	{
		if (search.length == 1)
		{
			return FindLast(string, search.data[0], ignoreCase);
		}

		if (search.length > 1 && search.length <= string.length)
		{
			String slice(string.data + string.length - search.length, search.length);
			for (size_t i = 0; i <= string.length - search.length; ++i)
			{
				if (Equals(slice, search, ignoreCase))
				{
					return static_cast<size_t>(slice.data - string.data);
				}

				slice.data -= 1;
			}
		}

		return SIZE_MAX;
	}

	bool StartsWith(String string, String prefix, bool ignoreCase)
	{
		if (prefix.length > string.length)
		{
			return false;
		}

		return Equals(Slice(string, 0, prefix.length), prefix, ignoreCase);
	}

	bool EndsWith(String string, String suffix, bool ignoreCase)
	{
		if (suffix.length > string.length)
		{
			return false;
		}

		return Equals(Slice(string, string.length - suffix.length), suffix, ignoreCase);
	}

	String Trim(String string)
	{
		size_t start = 0;
		size_t end = string.length;

		while (start < string.length && IsSpace(string.data[start]))
		{
			start += 1;
		}

		while (end > start && IsSpace(string.data[end - 1]))
		{
			end -= 1;
		}

		return Slice(string, start, end - start);
	}

	String TrimQuotes(String string)
	{
		size_t start = 0;
		size_t end = string.length;

		if (string.length != 0 && string.data[0] == '"')
		{
			start = 1;
		}

		if (string.length > 1 && string.data[string.length - 1] == '"')
		{
			end = string.length - 1;
		}

		return Slice(string, start, end - start);
	}

	bool operator==(String string, String other)
	{
		return string.length == other.length && CompareMemory(string.data, other.data, string.length) == 0;
	}

	bool operator!=(String string, String other)
	{
		return string.length != other.length || CompareMemory(string.data, other.data, string.length) != 0;
	}

	const char* begin(String string)
	{
		return string.data;
	}

	const char* end(String string)
	{
		return string.data + string.length;
	}

	bool ParseValue(String stream, bool& value)
	{
		if (stream.length == 0)
		{
			return false;
		}

		if (Equals(stream, "true", true) || Equals(stream, "1"))
		{
			value = true;
			return true;
		}

		if (Equals(stream, "false", true) || Equals(stream, "0"))
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

		constexpr uint64 maxDiv10 = UINT64_MAX / 10;
		constexpr uint64 maxMod10 = UINT64_MAX % 10;

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

		stream = Slice(stream, tokenStart);
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

		token = Slice(stream, 0, tokenEnd);
		stream = Slice(stream, tokenEnd);

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

	static bool Expand(StringBuilder& builder, size_t requiredCapacity)
	{
		for (StringBuilder::Chunk *chunk = builder.chunk.previous, *lastChunk = &builder.chunk; chunk; chunk = chunk->previous)
		{
			if (chunk->length == 0)
			{
				lastChunk->previous = chunk->previous;

				StringBuilder::Chunk archivedChunk = builder.chunk;

				builder.chunk = *chunk;
				*chunk = archivedChunk;
				builder.chunk.previous = chunk;

				return true;
			}

			lastChunk = chunk;
		}

		if (!builder.arena)
		{
			return false;
		}

		StringBuilder::Chunk* archivedChunk = nullptr;
		if (builder.chunk.capacity != 0)
		{
			archivedChunk = Push<StringBuilder::Chunk>(*builder.arena);
			*archivedChunk = builder.chunk;
		}

		constexpr size_t minCapacity = 64;
		constexpr size_t maxCapacityGrowth = 8096;

		builder.chunk.previous = archivedChunk;
		builder.chunk.capacity = Max(requiredCapacity, minCapacity, Min(builder.length, maxCapacityGrowth));
		builder.chunk.buffer = Push<char>(*builder.arena, builder.chunk.capacity);
		builder.chunk.length = 0;

		return true;
	}

	bool Append(StringBuilder& builder, char c)
	{
		if (builder.chunk.length + 1 > builder.chunk.capacity)
		{
			if (!Expand(builder, 1))
			{
				return false;
			}
		}

		builder.chunk.buffer[builder.chunk.length] = c;
		builder.chunk.length += 1;
		builder.length += 1;

		return true;
	}

	bool Append(StringBuilder& builder, String string)
	{
		while (string.length != 0)
		{
			size_t chunkRemaining = builder.chunk.capacity - builder.chunk.length;
			if (chunkRemaining == 0)
			{
				if (!Expand(builder, string.length))
				{
					return false;
				}

				chunkRemaining = builder.chunk.capacity;
			}

			size_t sliceLength = Min(string.length, chunkRemaining);

			CopyMemory(builder.chunk.buffer + builder.chunk.length, string.data, sliceLength);
			builder.chunk.length += sliceLength;
			builder.length += sliceLength;

			string = Slice(string, sliceLength);
		}

		return true;
	}

	bool Appendf(StringBuilder& builder, const char* format, ...)
	{
		va_list args;
		va_start(args, format);

		bool result = Appendv(builder, format, args);
		va_end(args);

		return result;
	}

	bool Appendv(StringBuilder& builder, const char* format, va_list args)
	{
		char buffer[STB_SPRINTF_MIN];

		int32 result = stbsp_vsprintfcb(
			[](const char* buffer, void* context, int32 length)
			{
				StringBuilder* builder = static_cast<StringBuilder*>(context);
				bool result = Append(*builder, String(buffer, static_cast<size_t>(length)));

				return result ? const_cast<char*>(buffer) : nullptr;
			},
			&builder, buffer, format, args);

		return (result >= 0 && static_cast<size_t>(result) < sizeof(buffer));
	}

	bool AppendLine(StringBuilder& builder, String line)
	{
		if (!Append(builder, line))
		{
			return false;
		}

		return Append(builder, '\n');
	}

	bool AppendLinef(StringBuilder& builder, const char* format, ...)
	{
		va_list args;
		va_start(args, format);

		bool result = Appendv(builder, format, args);
		va_end(args);

		return result && Append(builder, '\n');
	}

	bool AppendPath(StringBuilder& builder, String path)
	{
		char* lastChar = GetLastChar(builder);
		if (lastChar && *lastChar != '/' && *lastChar != '\\')
		{
			if (!Append(builder, '/'))
			{
				return false;
			}
		}

		return Append(builder, path);
	}

	bool AppendPathf(StringBuilder& builder, const char* format, ...)
	{
		char* lastChar = GetLastChar(builder);
		if (lastChar && *lastChar != '/' && *lastChar != '\\')
		{
			if (!Append(builder, '/'))
			{
				return false;
			}
		}

		va_list args;
		va_start(args, format);

		bool result = Appendv(builder, format, args);
		va_end(args);

		return result;
	}

	void NormalizePath(const StringBuilder& builder)
	{
		for (const StringBuilder::Chunk* chunk = &builder.chunk; chunk; chunk = chunk->previous)
		{
			char* start = chunk->buffer;
			char* end = chunk->buffer + chunk->length;

			for (char* c = start; c < end; ++c)
			{
				if (*c == '\\')
				{
					*c = '/';
				}
			}
		}
	}

	char* GetLastChar(const StringBuilder& builder)
	{
		if (builder.length != 0)
		{
			for (const StringBuilder::Chunk* chunk = &builder.chunk; chunk; chunk = chunk->previous)
			{
				if (chunk->length == 0)
				{
					continue;
				}

				return chunk->buffer + chunk->length - 1;
			}
		}

		return nullptr;
	}

	void Replace(const StringBuilder& builder, char oldChar, char newChar)
	{
		for (const StringBuilder::Chunk* chunk = &builder.chunk; chunk; chunk = chunk->previous)
		{
			for (size_t idx = 0; idx < chunk->length; ++idx)
			{
				char* c = chunk->buffer + idx;
				if (*c == oldChar)
				{
					*c = newChar;
				}
			}
		}
	}

	void Reset(StringBuilder& builder, size_t keepLength)
	{
		if (keepLength == 0)
		{
			for (StringBuilder::Chunk* chunk = &builder.chunk; chunk; chunk = chunk->previous)
			{
				chunk->length = 0;
			}

			builder.length = 0;
		}
		else if (keepLength < builder.length)
		{
			size_t discardLength = builder.length - keepLength;

			for (StringBuilder::Chunk *chunk = &builder.chunk, *lastChunk = nullptr; chunk; chunk = chunk->previous)
			{
				if (discardLength <= chunk->length)
				{
					chunk->length -= discardLength;

					if (lastChunk)
					{
						lastChunk->previous = chunk->previous;

						StringBuilder::Chunk archivedChunk = builder.chunk;

						builder.chunk = *chunk;
						*chunk = archivedChunk;
						builder.chunk.previous = chunk;
					}

					break;
				}

				discardLength -= chunk->length;
				chunk->length = 0;

				lastChunk = chunk;
			}

			builder.length = keepLength;
		}
	}

	String ToString(const StringBuilder& builder, Arena& arena, bool nullTerminate)
	{
		TSpan<char> buffer = Push<char>(arena, nullTerminate ? builder.length + 1 : builder.length);

		if (nullTerminate)
		{
			buffer[builder.length] = '\0';
		}

		char* bufferPtr = buffer.data + builder.length;
		for (const StringBuilder::Chunk* chunk = &builder.chunk; chunk; chunk = chunk->previous)
		{
			if (chunk->length != 0)
			{
				bufferPtr -= chunk->length;
				CopyMemory(bufferPtr, chunk->buffer, chunk->length);
			}
		}

		// Ensure the entire buffer has been filled
		BK_ASSERT(buffer.data == bufferPtr);

		return String(buffer.data, builder.length);
	}

	String GetDirectoryName(String path)
	{
		size_t slashIdx = FindLast(path, '/');
		if (slashIdx == SIZE_MAX)
		{
			slashIdx = FindLast(path, '\\');
		}

		return slashIdx != SIZE_MAX ? Slice(path, 0, slashIdx) : String::Empty;
	}

	String GetFileName(String path)
	{
		size_t slashIdx = FindLast(path, '/');
		if (slashIdx == SIZE_MAX)
		{
			slashIdx = FindLast(path, '\\');
		}

		return slashIdx != SIZE_MAX ? Slice(path, slashIdx + 1) : path;
	}

	String GetFileNameWithoutExtension(String path)
	{
		String fileName = GetFileName(path);
		size_t dotIdx = FindLast(fileName, '.');

		return dotIdx != SIZE_MAX ? Slice(fileName, 0, dotIdx) : fileName;
	}

	String GetExtension(String path)
	{
		String fileName = GetFileName(path);
		size_t dotIdx = FindLast(fileName, '.');

		return dotIdx != SIZE_MAX ? Slice(fileName, dotIdx + 1) : String::Empty;
	}
}
