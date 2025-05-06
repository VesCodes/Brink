#include "BkArena.h"

#include "BkMemory.h"

namespace Bk
{
	size_t Arena::DefaultBlockAlignment = BK_MEGABYTES(4);

	uint8* Arena::Push(size_t size, size_t alignment)
	{
		uintptr_t alignedOffset = AlignUp(blockOffset, alignment);
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

		blockOffset = alignedOffset + size;
		return reinterpret_cast<uint8*>(currentBlock) + alignedOffset;
	}

	uint8* Arena::PushZeroed(size_t size, size_t alignment)
	{
		uint8* result = Push(size, alignment);
		MemoryZero(result, size);

		return result;
	}
}
