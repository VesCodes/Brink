#include "Memory.h"

#include <stdlib.h>
#include <string.h>

namespace Bk
{
	void* AllocateMemory(size_t size)
	{
		return malloc(size);
	}

	void DeallocateMemory(void* ptr, size_t size)
	{
		free(ptr);
	}

	void* CopyMemory(void* dst, const void* src, size_t size)
	{
		return memcpy(dst, src, size);
	}

	void* MoveMemory(void* dst, const void* src, size_t size)
	{
		return memmove(dst, src, size);
	}

	int32 CompareMemory(const void* a, const void* b, size_t size)
	{
		return memcmp(a, b, size);
	}

	void* SetMemory(void* ptr, int32 value, size_t size)
	{
		return memset(ptr, value, size);
	}

	void* ZeroMemory(void* ptr, size_t size)
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

	thread_local struct
	{
		Arena scratchArenas[2] = {
			{ .blockAlignment = BK_MEGABYTES(4), .flags = ArenaFlags::KeepFirstBlock },
			{ .blockAlignment = BK_MEGABYTES(4), .flags = ArenaFlags::KeepFirstBlock },
		};
	} arenaTls;

	size_t Arena::DefaultAlignment = 8;
	size_t Arena::DefaultBlockAlignment = BK_MEGABYTES(4);

	TSpan<uint8> Push(Arena& arena, size_t size, size_t alignment)
	{
		size_t offset = arena.block ? AlignUp(arena.block->offset, alignment) : 0;
		if (!arena.block || offset + size > arena.block->size)
		{
			if (arena.blockAlignment == 0)
			{
				arena.blockAlignment = Arena::DefaultBlockAlignment;
			}

			offset = AlignUp(sizeof(ArenaBlock), alignment);

			size_t blockSize = AlignUp(offset + size, arena.blockAlignment);
			ArenaBlock* block = static_cast<ArenaBlock*>(AllocateMemory(blockSize));

			if (!block)
			{
				FatalError(1, "Failed to allocate %zd bytes for arena", blockSize);
			}

			block->previous = arena.block;
			block->size = blockSize;

			arena.block = block;
		}

		TSpan<uint8> result = {};
		result.data = reinterpret_cast<uint8*>(arena.block) + offset;
		result.length = size;

		arena.block->offset = offset + size;

		return result;
	}

	TSpan<uint8> PushZeroed(Arena& arena, size_t size, size_t alignment)
	{
		TSpan<uint8> result = Push(arena, size, alignment);
		ZeroMemory(result.data, result.length);

		return result;
	}

	ArenaMarker PushMarker(const Arena& arena)
	{
		ArenaMarker marker = {};
		if (arena.block)
		{
			marker.block = arena.block;
			marker.offset = arena.block->offset;
		}

		return marker;
	}

	void PopMarker(Arena& arena, ArenaMarker marker)
	{
		while (arena.block && arena.block != marker.block)
		{
			if (!arena.block->previous && EnumHasAnyFlags(arena.flags, ArenaFlags::KeepFirstBlock))
			{
				break;
			}

			ArenaBlock* block = arena.block;
			arena.block = block->previous;
			DeallocateMemory(block, block->size);
		}

		if (arena.block)
		{
			arena.block->offset = Max(marker.offset, sizeof(ArenaBlock));
		}
	}

	String Copy(Arena& arena, String source, bool nullTerminate)
	{
		TSpan<char> buffer = Push<char>(arena, nullTerminate ? source.length + 1 : source.length);
		CopyMemory(buffer.data, source.data, source.length);

		if (nullTerminate)
		{
			buffer.data[source.length] = '\0';
		}

		return String(buffer.data, source.length);
	}

	ArenaScope::ArenaScope(Arena& arena)
		: arena(arena), marker(PushMarker(arena))
	{
	}

	ArenaScope::~ArenaScope()
	{
		PopMarker(arena, marker);
	}

	ArenaScope GetScratchArena(const Arena* persistentArena)
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

	void Allocate(Arena& arena, BitArray& array, size_t length)
	{
		array.data = PushZeroed<uint32>(arena, (length + 31) / 32);
		array.length = length;
	}

	void Copy(BitArray dst, BitArray src)
	{
		BK_ASSERT(dst.length == src.length);

		if (dst.length > 0)
		{
			CopyMemory(dst.data, src.data, ((dst.length + 31) / 32) * sizeof(uint32));
		}
	}

	bool GetBit(BitArray array, size_t idx)
	{
		BK_ASSERT(idx < array.length);

		size_t wordIdx = idx / 32;
		return array.data[wordIdx] & (1u << (idx & 31));
	}

	void SetBit(BitArray array, size_t idx)
	{
		BK_ASSERT(idx < array.length);

		size_t wordIdx = idx / 32;
		array.data[wordIdx] |= (1u << (idx & 31));
	}

	void ClearBit(BitArray array, size_t idx)
	{
		BK_ASSERT(idx < array.length);

		size_t wordIdx = idx / 32;
		array.data[wordIdx] &= ~(1u << (idx & 31));
	}

	size_t FindBit(BitArray array, bool value, size_t start)
	{
		BK_ASSERT(start < array.length);

		size_t wordIdx = start / 32;
		size_t wordCount = (array.length + 31) / 32;

		uint32 test = value ? 0u : UINT32_MAX;
		uint32 mask = UINT32_MAX << (start & 31);

		while (wordIdx < wordCount && (array.data[wordIdx] & mask) == (test & mask))
		{
			wordIdx += 1;
			mask = UINT32_MAX;
		}

		if (wordIdx < wordCount)
		{
			uint32 bits = (value ? array.data[wordIdx] : ~array.data[wordIdx]) & mask;
			size_t lowestBit = wordIdx * 32 + CountTrailingZeros(bits);

			if (lowestBit < array.length)
			{
				return lowestBit;
			}
		}

		return SIZE_MAX;
	}
}
