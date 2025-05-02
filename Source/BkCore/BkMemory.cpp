#include "BkMemory.h"

#include <stdlib.h>
#include <string.h>

namespace Bk
{
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

	void* MemoryReserve(size_t size)
	{
		return malloc(size);
	}

	bool MemoryRelease(void* ptr, size_t size)
	{
		free(ptr);
		return true;
	}

	bool MemoryCommit(void* ptr, size_t size)
	{
		// #TODO: Check with malloc_usable_size?
		return true;
	}

	bool MemoryDecommit(void* ptr, size_t size)
	{
		return true;
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

	void Arena::Initialize(size_t size)
	{
		const size_t bytesToReserve = AlignUp(size, ReserveGranularity);

		data = static_cast<uint8*>(MemoryReserve(bytesToReserve));
		if (!data)
		{
			FatalError(1, "Failed to reserve %zd bytes for arena", bytesToReserve);
		}

		reservedSize = bytesToReserve;
		committedSize = 0;
		position = 0;
	}

	void Arena::Deinitialize()
	{
		MemoryRelease(data, reservedSize);

		data = nullptr;
		reservedSize = 0;
		committedSize = 0;
		position = 0;
	}

	uint8* Arena::Push(size_t size, size_t alignment)
	{
		const size_t alignedPosition = AlignUp(position, alignment);
		const size_t nextPosition = alignedPosition + size;

		if (nextPosition > committedSize)
		{
			if (nextPosition > reservedSize)
			{
				FatalError(1, "Failed to commit %zd bytes for arena, requested %zd bytes more than available", nextPosition - position, nextPosition - reservedSize);
			}

			size_t bytesToCommit = AlignUp(nextPosition - committedSize, CommitGranularity);
			if (committedSize + bytesToCommit > reservedSize)
			{
				bytesToCommit = reservedSize - committedSize;
			}

			BK_ASSERT(MemoryCommit(data + committedSize, bytesToCommit));
			committedSize += bytesToCommit;
		}

		position = nextPosition;
		return data + alignedPosition;
	}

	uint8* Arena::PushZeroed(size_t size, size_t alignment)
	{
		uint8* result = Push(size, alignment);
		MemoryZero(result, size);

		return result;
	}

	void Arena::Pop(size_t size)
	{
		position = (size < position) ? position - size : 0;

		const size_t alignedPosition = AlignUp(position, CommitGranularity);
		if (alignedPosition + DecommitThreshold <= committedSize)
		{
			const size_t bytesToDecommit = committedSize - alignedPosition;

			BK_ASSERT(MemoryDecommit(data + alignedPosition, bytesToDecommit));
			committedSize -= bytesToDecommit;
		}
	}
}
