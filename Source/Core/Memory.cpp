#include "Memory.h"

#include <stdlib.h>
#include <string.h>

namespace Bk
{
	void* MemoryAllocate(size_t size)
	{
		return malloc(size);
	}

	void MemoryDeallocate(void* ptr, size_t size)
	{
		free(ptr);
	}

	void* MemoryCopy(void* dst, const void* src, size_t size)
	{
		return memcpy(dst, src, size);
	}

	void* MemoryMove(void* dst, const void* src, size_t size)
	{
		return memmove(dst, src, size);
	}

	int32 MemoryCompare(const void* a, const void* b, size_t size)
	{
		return memcmp(a, b, size);
	}

	void* MemorySet(void* ptr, int32 value, size_t size)
	{
		return memset(ptr, value, size);
	}

	void* MemoryZero(void* ptr, size_t size)
	{
		return memset(ptr, 0, size);
	}

	uint32 CountLeadingZeros(uint64 value)
	{
		return value ? static_cast<uint32>(__builtin_clzll(value)) : 64;
	}

	uint32 CountTrailingZeros(uint64 value)
	{
		return value ? static_cast<uint32>(__builtin_ctzll(value)) : 64;
	}

	bool BitsetIsSet(const uint32* bitset, size_t index)
	{
		size_t wordIdx = index / 32;
		return bitset[wordIdx] & (1u << (index & 31));
	}

	void BitsetSet(uint32* bitset, size_t index)
	{
		size_t wordIdx = index / 32;
		bitset[wordIdx] |= (1u << (index & 31));
	}

	void BitsetUnset(uint32* bitset, size_t index)
	{
		size_t wordIdx = index / 32;
		bitset[wordIdx] &= ~(1u << (index & 31));
	}

	size_t BitsetFind(const uint32* bitset, bool value, size_t offset, size_t length)
	{
		BK_ASSERT(offset < length);

		size_t wordIdx = offset / 32;
		size_t wordCount = (length + 31) / 32;

		uint32 test = value ? 0u : UINT32_MAX;
		uint32 mask = UINT32_MAX << (offset & 31);

		while (wordIdx < wordCount && (bitset[wordIdx] & mask) == (test & mask))
		{
			wordIdx += 1;
			mask = UINT32_MAX;
		}

		if (wordIdx < wordCount)
		{
			uint32 bits = (value ? bitset[wordIdx] : ~bitset[wordIdx]) & mask;
			size_t lowestBit = wordIdx * 32 + CountTrailingZeros(bits);

			if (lowestBit < length)
			{
				return lowestBit;
			}
		}

		return SIZE_MAX;
	}
}
