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

	thread_local struct
	{
		Arena scratchArenas[2] = {
			{ .blockAlignment = BK_MEGABYTES(4), .flags = ArenaFlags::KeepFirstBlock },
			{ .blockAlignment = BK_MEGABYTES(4), .flags = ArenaFlags::KeepFirstBlock },
		};
	} arenaTls;

	size_t Arena::DefaultAlignment = 8;
	size_t Arena::DefaultBlockAlignment = BK_MEGABYTES(4);

	TSpan<uint8> Arena::Push(size_t size, size_t alignment)
	{
		size_t alignedOffset = currentBlock ? AlignUp(currentBlock->offset, alignment) : 0;
		if (!currentBlock || alignedOffset + size > currentBlock->size)
		{
			if (blockAlignment == 0)
			{
				blockAlignment = DefaultBlockAlignment;
			}

			alignedOffset = AlignUp(sizeof(ArenaBlock), alignment);

			size_t blockSize = AlignUp(alignedOffset + size, blockAlignment);
			ArenaBlock* block = static_cast<ArenaBlock*>(MemoryAllocate(blockSize));

			if (!block)
			{
				FatalError(1, "Failed to allocate %zd bytes for arena", blockSize);
			}

			block->previous = currentBlock;
			block->size = blockSize;

			currentBlock = block;
		}

		TSpan<uint8> result = {};
		result.data = reinterpret_cast<uint8*>(currentBlock) + alignedOffset;
		result.length = size;

		currentBlock->offset = alignedOffset + size;

		return result;
	}

	TSpan<uint8> Arena::PushZeroed(size_t size, size_t alignment)
	{
		TSpan<uint8> result = Push(size, alignment);
		MemoryZero(result.data, result.length);

		return result;
	}

	ArenaMarker Arena::PushMarker() const
	{
		ArenaMarker marker = {};
		if (currentBlock)
		{
			marker.block = currentBlock;
			marker.offset = currentBlock->offset;
		}

		return marker;
	}

	void Arena::PopMarker(ArenaMarker marker)
	{
		while (currentBlock && currentBlock != marker.block)
		{
			if (!currentBlock->previous && EnumHasAnyFlags(flags, ArenaFlags::KeepFirstBlock))
			{
				break;
			}

			ArenaBlock* block = currentBlock;
			currentBlock = currentBlock->previous;
			MemoryDeallocate(block, block->size);
		}

		if (currentBlock)
		{
			currentBlock->offset = Max(marker.offset, sizeof(ArenaBlock));
		}
	}

	ArenaScope::ArenaScope(Arena& arena)
		: arena(arena), marker(arena.PushMarker())
	{
	}

	ArenaScope::~ArenaScope()
	{
		arena.PopMarker(marker);
	}

	ArenaScope GetScratchArena(Arena* persistentArena)
	{
		for (Arena& scratchArena : arenaTls.scratchArenas)
		{
			if (&scratchArena != persistentArena)
			{
				return scratchArena;
			}
		}

		FatalError(1, "Failed to acquire scratch arena");
	}
}
