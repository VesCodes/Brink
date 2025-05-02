#pragma once

#include "BkCore.h"

#include <stdarg.h>

namespace Bk
{
	int32 StringPrintf(char* dst, size_t dstLength, const char* format, ...);
	int32 StringPrintv(char* dst, size_t dstLength, const char* format, va_list args);

	struct String
	{
		String() = default;

		constexpr String(const char* string, size_t length);

		template<size_t N>
		constexpr String(const char (&string)[N]);

		String Slice(size_t offset, size_t count = SIZE_MAX) const;

		char operator[](size_t index) const;

		const char* begin() const;
		const char* end() const;

		const char* data;
		size_t length;
	};
}

namespace Bk
{
	constexpr String::String(const char* string, size_t length)
		: data(string), length(length)
	{
	}

	template<size_t N>
	constexpr String::String(const char (&string)[N])
		: data(string), length(N - 1)
	{
	}
}
