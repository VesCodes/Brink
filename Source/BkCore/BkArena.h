#pragma once

#include "BkCore.h"

namespace Bk
{
	struct ArenaBlock
	{
		ArenaBlock* previous;
		size_t size;
		size_t offset;
	};

	struct ArenaMarker
	{
		ArenaBlock* block;
		size_t offset;
	};

	struct Arena
	{
		static size_t DefaultAlignment;
		static size_t DefaultBlockAlignment;

		ArenaMarker GetMarker() const;
		void SetMarker(ArenaMarker marker);

		uint8* Push(size_t size, size_t alignment = DefaultAlignment);
		uint8* PushZeroed(size_t size, size_t alignment = DefaultAlignment);

		template<typename Type>
		Type* Push(size_t count = 1);

		template<typename Type>
		Type* PushZeroed(size_t count = 1);

		ArenaBlock* currentBlock;
		size_t blockAlignment;
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
