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

		template<typename OtherType>
		TSpan(TSpan<OtherType> other) requires(__is_convertible(OtherType*, Type*));

		operator Type*() const;
		Type& operator[](size_t idx) const;

		Type* data;
		size_t length;
	};

	template<typename Type>
	TSpan<Type> Slice(TSpan<Type> span, size_t start, size_t count = SIZE_MAX);

	template<typename Type>
	Type* begin(TSpan<Type> span);

	template<typename Type>
	Type* end(TSpan<Type> span);

	template<typename Type>
	auto AsBytes(Type* data, size_t length);

	template<typename Type>
	TSpan<const uint8> AsBytes(std::initializer_list<Type> data);

	template<typename Type, size_t N>
	auto AsBytes(Type (&data)[N]);

	template<typename Type>
	auto AsBytes(TSpan<Type> data);
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
	template<typename OtherType>
	TSpan<Type>::TSpan(TSpan<OtherType> other) requires(__is_convertible(OtherType*, Type*))
		: data(other.data), length(other.length)
	{
	}

	template<typename Type>
	TSpan<Type>::operator Type*() const
	{
		return data;
	}

	template<typename Type>
	Type& TSpan<Type>::operator[](size_t idx) const
	{
		BK_ASSERT(idx < length);
		return data[idx];
	}

	template<typename Type>
	TSpan<Type> Slice(TSpan<Type> span, size_t start, size_t count)
	{
		BK_ASSERT(start < span.length);
		return TSpan(span.data + start, Min(count, span.length - start));
	}

	template<typename Type>
	Type* begin(TSpan<Type> span)
	{
		return span.data;
	}

	template<typename Type>
	Type* end(TSpan<Type> span)
	{
		return span.data + span.length;
	}

	template<typename Type>
	auto AsBytes(Type* data, size_t length)
	{
		if constexpr (__is_const(Type))
		{
			return TSpan<const uint8>(reinterpret_cast<const uint8*>(data), length * sizeof(Type));
		}
		else
		{
			return TSpan<uint8>(reinterpret_cast<uint8*>(data), length * sizeof(Type));
		}
	}

	template<typename Type>
	TSpan<const uint8> AsBytes(std::initializer_list<Type> data)
	{
		return AsBytes(data.begin(), data.size());
	}

	template<typename Type, size_t N>
	auto AsBytes(Type (&data)[N])
	{
		return AsBytes(data, N);
	}

	template<typename Type>
	auto AsBytes(TSpan<Type> data)
	{
		return AsBytes(data.data, data.length);
	}
}
