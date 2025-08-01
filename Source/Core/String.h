#pragma once

#include "Core.h"

#include <stdarg.h>

namespace Bk
{
	struct Arena;

	constexpr bool IsSpace(char c);
	constexpr bool IsAlpha(char c);
	constexpr bool IsDigit(char c);
	constexpr bool IsHexDigit(char c);

	constexpr char ToLower(char c);
	constexpr char ToUpper(char c);

	struct String
	{
		static const String Empty;

		String() = default;

		constexpr String(const char* string, size_t length);
		constexpr String(const char* string);

		String Slice(size_t start, size_t count = SIZE_MAX) const;
		String Range(size_t start, size_t end) const;

		bool Equals(String other, bool ignoreCase = false) const;

		bool Contains(char search, bool ignoreCase = false) const;
		bool Contains(String search, bool ignoreCase = false) const;

		size_t Find(char search, bool ignoreCase = false) const;
		size_t Find(String search, bool ignoreCase = false) const;
		size_t FindLast(char search, bool ignoreCase = false) const;
		size_t FindLast(String search, bool ignoreCase = false) const;

		bool StartsWith(String prefix, bool ignoreCase = false) const;
		bool EndsWith(String suffix, bool ignoreCase = false) const;

		String Trim() const;
		String TrimQuotes() const;

		char operator[](size_t index) const;
		bool operator==(String other) const;
		bool operator!=(String other) const;

		const char* begin() const;
		const char* end() const;

		const char* data;
		size_t length;
	};

	bool ParseValue(String stream, bool& value);
	bool ParseValue(String stream, int64& value);
	bool ParseValue(String stream, int32& value);
	bool ParseValue(String stream, uint64& value);
	bool ParseValue(String stream, uint32& value);
	bool ParseValue(String stream, double& value);
	bool ParseValue(String stream, float& value);
	bool ParseToken(String& stream, String& token);

	struct StringBuilder
	{
		StringBuilder(char* buffer, size_t bufferSize);
		StringBuilder(Arena& arena);

		bool Append(char c);
		bool Append(String string);
		bool Appendf(const char* format, ...);
		bool Appendv(const char* format, va_list args);

		bool AppendLine(String line);
		bool AppendLinef(const char* format, ...);

		bool AppendPath(String path);
		bool AppendPathf(const char* format, ...);

		bool Expand(size_t requiredCapacity);
		void Reset(size_t keepLength = 0);

		String ToString(Arena& arena, bool nullTerminate = false) const;

		Arena* arena;

		struct Chunk
		{
			Chunk* previous;
			char* buffer;
			size_t capacity;
			size_t length;
		} chunk;

		size_t length;
	};

	String GetDirectoryName(String path);
	String GetFileName(String path);
	String GetFileNameWithoutExtension(String path);
	String GetExtension(String path);

	template<size_t BufferSize>
	struct TStringBuilder : StringBuilder
	{
		TStringBuilder();
		TStringBuilder(Arena& arena);

		char buffer[BufferSize];
	};

	struct AsciiSet
	{
		template<size_t N>
		constexpr AsciiSet(const char (&chars)[N]);

		constexpr bool Contains(char c) const;

		uint64 loMask;
		uint64 hiMask;
	};
}

namespace Bk
{
	constexpr bool IsSpace(char c)
	{
		return (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v');
	}

	constexpr bool IsAlpha(char c)
	{
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
	}

	constexpr bool IsDigit(char c)
	{
		return (c >= '0' && c <= '9');
	}

	constexpr bool IsHexDigit(char c)
	{
		return IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
	}

	constexpr char ToLower(char c)
	{
		return (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
	}

	constexpr char ToUpper(char c)
	{
		return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
	}

	constexpr String::String(const char* string, size_t length)
		: data(string), length(length)
	{
	}

	constexpr String::String(const char* string)
		: data(string), length(__builtin_strlen(string))
	{
	}

	template<size_t BufferSize>
	TStringBuilder<BufferSize>::TStringBuilder()
		: StringBuilder(buffer, BufferSize)
	{
	}

	template<size_t BufferSize>
	TStringBuilder<BufferSize>::TStringBuilder(Arena& arena)
		: StringBuilder(arena)
	{
		chunk.buffer = buffer;
		chunk.capacity = BufferSize;
	}

	template<size_t N>
	constexpr AsciiSet::AsciiSet(const char (&chars)[N])
	{
		loMask = 0;
		hiMask = 0;

		for (size_t i = 0; i < N - 1; ++i)
		{
			char c = chars[i];

			uint64 bit = 1ull << (c & 0x3f);
			uint64 isLo = 0ull - (c >> 6 == 0);
			uint64 isHi = 0ull - (c >> 6 == 1);

			loMask |= bit & isLo;
			hiMask |= bit & isHi;
		}
	}

	constexpr bool AsciiSet::Contains(char c) const
	{
		uint64 bit = 1ull << (c & 0x3f);
		uint64 isLo = 0ull - (c >> 6 == 0);
		uint64 isHi = 0ull - (c >> 6 == 1);

		return ((bit & isLo & loMask) | (bit & isHi & hiMask)) != 0;
	}
}
