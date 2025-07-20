#pragma once

#include "Core.h"
#include "Span.h"

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

		TSpan<uint8> Push(size_t size, size_t alignment = DefaultAlignment);
		TSpan<uint8> PushZeroed(size_t size, size_t alignment = DefaultAlignment);

		template<typename Type>
		TSpan<Type> Push(size_t count = 1);

		template<typename Type>
		TSpan<Type> PushZeroed(size_t count = 1);

		ArenaMarker PushMarker() const;
		void PopMarker(ArenaMarker marker);

		ArenaBlock* currentBlock;
		size_t blockAlignment;
	};

	struct ScratchArena
	{
		static ScratchArena Get(Arena* persistentArena = nullptr);

		~ScratchArena();

		Arena* arena;
		ArenaMarker marker;
	};
}

namespace Bk
{
	template<typename Type>
	TSpan<Type> Arena::Push(size_t count)
	{
		uint8* data = Push(sizeof(Type) * count, alignof(Type));

		TSpan<Type> result = {};
		result.data = reinterpret_cast<Type*>(data);
		result.length = count;

		return result;
	}

	template<typename Type>
	TSpan<Type> Arena::PushZeroed(size_t count)
	{
		uint8* data = PushZeroed(sizeof(Type) * count, alignof(Type));

		TSpan<Type> result = {};
		result.data = reinterpret_cast<Type*>(data);
		result.length = count;

		return result;
	}
}
