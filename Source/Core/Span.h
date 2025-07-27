#pragma once

#include "Core.h"

#include <initializer_list>

namespace Bk
{
	template<typename Type>
	struct TSpan
	{
		TSpan() = default;

		TSpan(Type* data, size_t length);
		TSpan(std::initializer_list<Type> data);

		template<size_t N>
		TSpan(Type (&data)[N]);

		TSpan Slice(size_t start, size_t count = SIZE_MAX) const;

		operator Type*() const;
		Type& operator[](size_t index) const;

		Type* begin() const;
		Type* end() const;

		Type* data;
		size_t length;
	};
}

namespace Bk
{
	template<typename Type>
	TSpan<Type>::TSpan(Type* data, size_t length)
		: data(data), length(length)
	{
	}

	template<typename Type>
	TSpan<Type>::TSpan(std::initializer_list<Type> data)
		: data(const_cast<Type*>(data.begin())), length(data.size())
	{
	}

	template<typename Type>
	template<size_t N>
	TSpan<Type>::TSpan(Type (&data)[N])
		: data(data), length(N)
	{
	}

	template<typename Type>
	TSpan<Type> TSpan<Type>::Slice(size_t start, size_t count) const
	{
		BK_ASSERT(start >= 0 && start < length);
		return TSpan(data + start, Min(count, length - start));
	}

	template<typename Type>
	TSpan<Type>::operator Type*() const
	{
		return data;
	}

	template<typename Type>
	Type& TSpan<Type>::operator[](size_t index) const
	{
		BK_ASSERT(index >= 0 && index < length);
		return data[index];
	}

	template<typename Type>
	Type* TSpan<Type>::begin() const
	{
		return data;
	}

	template<typename Type>
	Type* TSpan<Type>::end() const
	{
		return data + length;
	}
}
