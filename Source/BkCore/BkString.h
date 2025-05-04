#pragma once

#include "BkCore.h"

#include <stdarg.h>

namespace Bk
{
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

		bool Equals(String other, bool ignoreCase = false) const;

		bool Contains(char search, bool ignoreCase = false) const;
		bool Contains(String search, bool ignoreCase = false) const;

		size_t Find(char search, bool ignoreCase = false) const;
		size_t Find(String search, bool ignoreCase = false) const;

		char operator[](size_t index) const;

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
}

namespace Bk
{
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
}
