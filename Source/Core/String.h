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

		char operator[](size_t idx) const;

		const char* data;
		size_t length;
	};

	String Slice(String string, size_t start, size_t count = SIZE_MAX);

	bool Equals(String string, String other, bool ignoreCase = false);

	bool Contains(String string, char search, bool ignoreCase = false);
	bool Contains(String string, String search, bool ignoreCase = false);

	size_t Find(String string, char search, bool ignoreCase = false);
	size_t Find(String string, String search, bool ignoreCase = false);
	size_t FindLast(String string, char search, bool ignoreCase = false);
	size_t FindLast(String string, String search, bool ignoreCase = false);

	bool StartsWith(String string, String prefix, bool ignoreCase = false);
	bool EndsWith(String string, String suffix, bool ignoreCase = false);

	String Trim(String string);
	String TrimQuotes(String string);

	bool operator==(String string, String other);
	bool operator!=(String string, String other);

	const char* begin(String string);
	const char* end(String string);

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
		StringBuilder() = default;

		StringBuilder(char* buffer, size_t bufferSize);
		StringBuilder(Arena& arena);

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

	bool Append(StringBuilder& builder, char c);
	bool Append(StringBuilder& builder, String string);
	bool Appendf(StringBuilder& builder, const char* format, ...);
	bool Appendv(StringBuilder& builder, const char* format, va_list args);

	bool AppendLine(StringBuilder& builder, String line);
	bool AppendLinef(StringBuilder& builder, const char* format, ...);

	bool AppendPath(StringBuilder& builder, String path);
	bool AppendPathf(StringBuilder& builder, const char* format, ...);
	void NormalizePath(const StringBuilder& builder);

	char* GetLastChar(const StringBuilder& builder);
	void Replace(const StringBuilder& builder, char oldChar, char newChar);

	void Reset(StringBuilder& builder, size_t keepLength = 0);

	String ToString(const StringBuilder& builder, Arena& arena, bool nullTerminate = false);

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
		AsciiSet() = default;

		template<size_t N>
		constexpr AsciiSet(const char (&chars)[N]);

		uint64 loMask;
		uint64 hiMask;
	};

	constexpr bool Contains(const AsciiSet& set, char c);
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

	constexpr bool Contains(const AsciiSet& set, char c)
	{
		uint64 bit = 1ull << (c & 0x3f);
		uint64 isLo = 0ull - (c >> 6 == 0);
		uint64 isHi = 0ull - (c >> 6 == 1);

		return ((bit & isLo & set.loMask) | (bit & isHi & set.hiMask)) != 0;
	}
}
