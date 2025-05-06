#pragma once

#include "BkCore.h"

#define BK_KILOBYTES(x) ((x) << 10)
#define BK_MEGABYTES(x) ((x) << 20)
#define BK_GIGABYTES(x) ((x) << 30)
#define BK_TERABYTES(x) ((x) << 40)

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment);

	void* MemoryAllocate(size_t size);
	void MemoryDeallocate(void* ptr, size_t size);

	void* MemoryCopy(void* dst, const void* src, size_t size);
	void* MemoryMove(void* dst, const void* src, size_t size);
	int32 MemoryCompare(const void* a, const void* b, size_t size);
	void* MemorySet(void* ptr, int32 value, size_t size);
	void* MemoryZero(void* ptr, size_t size);

	uint32 CountLeadingZeros(uint64 value);
	uint32 CountTrailingZeros(uint64 value);

	bool BitsetIsSet(const uint32* bitset, size_t index);
	void BitsetSet(uint32* bitset, size_t index);
	void BitsetUnset(uint32* bitset, size_t index);
	size_t BitsetFindNextSet(const uint32* bitset, size_t bitsetLength, size_t index);
	size_t BitsetFindNextUnset(const uint32* bitset, size_t bitsetLength, size_t index);
}

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment)
	{
		return T(((uintptr_t)value + alignment - 1) & ~(alignment - 1));
	}
}
