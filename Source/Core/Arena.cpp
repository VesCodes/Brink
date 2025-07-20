#include "Arena.h"

#include "Memory.h"

namespace Bk
{
	thread_local struct
	{
		Arena scratchArenas[2];
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
			ArenaBlock* block = currentBlock;
			currentBlock = currentBlock->previous;
			MemoryDeallocate(block, block->size);
		}

		if (currentBlock)
		{
			BK_ASSERT(marker.offset >= sizeof(ArenaBlock));
			currentBlock->offset = marker.offset;
		}
	}

	ScratchArena ScratchArena::Get(Arena* persistentArena)
	{
		ScratchArena result = {};

		for (Arena& scratchArena : arenaTls.scratchArenas)
		{
			if (&scratchArena != persistentArena)
			{
				result.arena = &scratchArena;
				result.marker = scratchArena.PushMarker();
				break;
			}
		}

		return result;
	}

	ScratchArena::~ScratchArena()
	{
		if (arena)
		{
			arena->PopMarker(marker);
			arena = nullptr;
		}
	}
}
