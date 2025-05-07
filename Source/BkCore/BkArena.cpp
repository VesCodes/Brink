#include "BkArena.h"

#include "BkMemory.h"

namespace Bk
{
	size_t Arena::DefaultAlignment = 8;
	size_t Arena::DefaultBlockAlignment = BK_MEGABYTES(4);

	ArenaMarker Arena::GetMarker() const
	{
		ArenaMarker marker = {};
		if (currentBlock)
		{
			marker.block = currentBlock;
			marker.offset = currentBlock->offset;
		}

		return marker;
	}

	void Arena::SetMarker(ArenaMarker marker)
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

	uint8* Arena::Push(size_t size, size_t alignment)
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
				return nullptr;
			}

			block->previous = currentBlock;
			block->size = blockSize;

			currentBlock = block;
		}

		currentBlock->offset = alignedOffset + size;
		return reinterpret_cast<uint8*>(currentBlock) + alignedOffset;
	}

	uint8* Arena::PushZeroed(size_t size, size_t alignment)
	{
		uint8* result = Push(size, alignment);
		MemoryZero(result, size);

		return result;
	}
}
