#pragma once

#include "BkCore.h"

#include <stdarg.h>

namespace Bk
{
	constexpr bool IsSpace(char c);
	constexpr bool IsAlpha(char c);
	constexpr bool IsDigit(char c);
	constexpr bool IsHexDigit(char c);

	constexpr char ToLower(char c);
	constexpr char ToUpper(char c);

	constexpr size_t StringLength(const char* string);

	int32 StringPrintf(char* dst, size_t dstLength, const char* format, ...);
	int32 StringPrintv(char* dst, size_t dstLength, const char* format, va_list args);

	struct String
	{
		String() = default;

		constexpr String(const char* string, size_t length);
		constexpr String(const char* string);

		String Slice(size_t offset, size_t count = SIZE_MAX) const;
		String Range(size_t start, size_t end) const;

		bool Equals(String other, bool ignoreCase = false) const;

		bool Contains(char search, bool ignoreCase = false) const;
		bool Contains(String search, bool ignoreCase = false) const;

		size_t Find(char search, bool ignoreCase = false) const;
		size_t Find(String search, bool ignoreCase = false) const;
		size_t FindLast(char search, bool ignoreCase = false) const;
		size_t FindLast(String search, bool ignoreCase = false) const;

		bool Parse(bool& value) const;
		bool Parse(int64& value) const;
		bool Parse(int32& value) const;
		bool Parse(uint64& value) const;
		bool Parse(uint32& value) const;
		bool Parse(double& value) const;
		bool Parse(float& value) const;

		char operator[](size_t index) const;
		bool operator==(String other) const;
		bool operator!=(String other) const;

		const char* begin() const;
		const char* end() const;

		const char* data;
		size_t length;
	};

	struct StringBuffer
	{
		StringBuffer(char* buffer, size_t bufferSize);

		bool Append(char c);
		bool Append(String string);
		bool Appendf(const char* format, ...);
		bool Appendv(const char* format, va_list args);

		void Reset();

		operator String() const;

		char* data;
		size_t length;
		size_t capacity;
	};

	template<size_t BufferSize>
	struct TStringBuffer : StringBuffer
	{
		TStringBuffer();
		TStringBuffer(String string);

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

	constexpr size_t StringLength(const char* string)
	{
		return __builtin_strlen(string);
	}

	constexpr String::String(const char* string, size_t length)
		: data(string), length(length)
	{
	}

	constexpr String::String(const char* string)
		: data(string), length(StringLength(string))
	{
	}

	template<size_t BufferSize>
	TStringBuffer<BufferSize>::TStringBuffer()
		: StringBuffer(buffer, BufferSize)
	{
	}

	template<size_t BufferSize>
	TStringBuffer<BufferSize>::TStringBuffer(String string)
		: StringBuffer(buffer, BufferSize)
	{
		Append(string);
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
