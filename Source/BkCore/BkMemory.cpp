#include "BkMemory.h"

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

	size_t BitsetFindNextSet(const uint32* bitset, size_t bitsetLength, size_t index)
	{
		size_t wordCount = (bitsetLength + 31) / 32;
		size_t wordIdx = index / 32;

		// Mask out bits before index in first word
		uint32 word = bitset[wordIdx] & (UINT32_MAX << (index & 31));

		for (; wordIdx < wordCount; ++wordIdx, word = bitset[wordIdx])
		{
			if (word)
			{
				return wordIdx * 32 + CountTrailingZeros(word);
			}
		}

		return bitsetLength;
	}

	size_t BitsetFindNextUnset(const uint32* bitset, size_t bitsetLength, size_t index)
	{
		size_t wordCount = (bitsetLength + 31) / 32;
		size_t wordIdx = index / 32;

		// Mask out bits before index in first word
		uint32 word = ~bitset[wordIdx] & (UINT32_MAX << (index & 31));

		for (; wordIdx < wordCount; ++wordIdx, word = ~bitset[wordIdx])
		{
			if (word)
			{
				return wordIdx * 32 + CountTrailingZeros(word);
			}
		}

		return bitsetLength;
	}
}
