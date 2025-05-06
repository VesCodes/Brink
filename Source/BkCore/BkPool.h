#pragma once

#include "BkArena.h"
#include "BkCore.h"
#include "BkMemory.h"

namespace Bk
{
	template<typename Type>
	struct TPool
	{
		static_assert(sizeof(Type) >= sizeof(uintptr_t), "Pool type must be at least the size of a pointer");

		void Initialize(Arena* arena, uint16 size);

		Type* AllocateItem(uint32* handle = nullptr);
		void FreeItem(uint32 handle);

		uint32 GetHandle(Type* item);
		Type* GetItem(uint32 handle);

		Type* items;
		uint16* generations;
		uint32* alive;
		uintptr_t nextFree;
		size_t capacity;
		size_t count;
	};
}

namespace Bk
{
	template<typename Type>
	void TPool<Type>::Initialize(Arena* arena, uint16 size)
	{
		capacity = size;
		count = 0;

		items = arena->Push<Type>(capacity);
		generations = arena->PushZeroed<uint16>(capacity);
		alive = arena->PushZeroed<uint32>((capacity + 31) / 32);
		nextFree = 0;
	}

	template<typename Type>
	Type* TPool<Type>::AllocateItem(uint32* handle)
	{
		Type* item = nullptr;
		size_t index;

		if (nextFree)
		{
			item = reinterpret_cast<Type*>(nextFree);
			index = static_cast<size_t>(item - items);

			nextFree = *reinterpret_cast<uintptr_t*>(nextFree);
		}
		else if (count < capacity)
		{
			item = items + count;
			index = count;

			count += 1;
		}

		if (item)
		{
			uint16& generation = generations[index];
			if (generation == 0)
			{
				generation = 1;
			}

			BitsetSet(alive, index);

			if (handle)
			{
				*handle = (static_cast<uint32>(generation) << 16) | index;
			}
		}

		return item;
	}

	template<typename Type>
	void TPool<Type>::FreeItem(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		generations[index] += 1;
		BitsetUnset(alive, index);

		Type* item = items + index;
		*reinterpret_cast<uintptr_t*>(item) = nextFree;
		nextFree = reinterpret_cast<uintptr_t>(item);
	}

	template<typename Type>
	uint32 TPool<Type>::GetHandle(Type* item)
	{
		size_t index = item - items;
		BK_ASSERT(index < count);

		uint16 generation = generations[index];

		return (static_cast<uint32>(generation) << 16) | index;
	}

	template<typename Type>
	Type* TPool<Type>::GetItem(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		return items + index;
	}
}
