#pragma once

#include "BkCore.h"

namespace Bk
{
	struct ArenaBlock
	{
		ArenaBlock* previous;
		size_t size;
	};

	struct Arena
	{
		static uintptr_t DefaultBlockAlignment;

		uint8* Push(size_t size, size_t alignment);
		uint8* PushZeroed(size_t size, size_t alignment);

		template<typename Type>
		Type* Push(size_t count = 1);

		template<typename Type>
		Type* PushZeroed(size_t count = 1);

		ArenaBlock* currentBlock;
		uintptr_t blockOffset;
		uintptr_t blockAlignment;
	};
}

namespace Bk
{
	template<typename Type>
	Type* Arena::Push(size_t count)
	{
		uint8* result = Push(sizeof(Type) * count, alignof(Type));
		return reinterpret_cast<Type*>(result);
	}

	template<typename Type>
	Type* Arena::PushZeroed(size_t count)
	{
		uint8* result = PushZeroed(sizeof(Type) * count, alignof(Type));
		return reinterpret_cast<Type*>(result);
	}
}
