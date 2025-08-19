#pragma once

#include "Core.h"
#include "Memory.h"

namespace Bk
{
	template<typename Type>
	struct TPoolIterator;

	template<typename Type>
	struct TPool
	{
		static_assert(sizeof(Type) >= sizeof(uintptr_t), "Pool type must be at least the size of a pointer");

		void Initialize(Arena& arena, uint16 size);

		Type* Acquire(uint32* handle = nullptr);
		void Release(uint32 handle);

		Type* Get(uint32 handle);
		uint32 GetHandle(Type* slot);

		TPoolIterator<Type> begin() const;
		TPoolIterator<Type> end() const;

		Type* slots;
		uint16* generations;
		uint32* alive;
		uintptr_t nextFree;
		size_t capacity;
		size_t count;
	};

	template<typename Type>
	struct TPoolIterator
	{
		TPoolIterator(const TPool<Type>& pool, size_t start);

		TPoolIterator& operator++();
		Type* operator*() const;
		bool operator!=(const TPoolIterator& other) const;

		const TPool<Type>& pool;
		size_t index;
	};
}

namespace Bk
{
	template<typename Type>
	void TPool<Type>::Initialize(Arena& arena, uint16 size)
	{
		capacity = size;
		count = 0;

		slots = arena.Push<Type>(capacity);
		generations = arena.PushZeroed<uint16>(capacity);
		alive = arena.PushZeroed<uint32>((capacity + 31) / 32);
		nextFree = 0;
	}

	template<typename Type>
	Type* TPool<Type>::Acquire(uint32* handle)
	{
		Type* slot = nullptr;
		size_t index;

		if (nextFree)
		{
			slot = reinterpret_cast<Type*>(nextFree);
			index = static_cast<size_t>(slot - slots);

			nextFree = *reinterpret_cast<uintptr_t*>(nextFree);
		}
		else if (count < capacity)
		{
			slot = slots + count;
			index = count;

			count += 1;
		}

		if (slot)
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

		return slot;
	}

	template<typename Type>
	void TPool<Type>::Release(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		generations[index] += 1;
		BitsetUnset(alive, index);

		Type* slot = slots + index;
		*reinterpret_cast<uintptr_t*>(slot) = nextFree;
		nextFree = reinterpret_cast<uintptr_t>(slot);
	}

	template<typename Type>
	Type* TPool<Type>::Get(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		return slots + index;
	}

	template<typename Type>
	uint32 TPool<Type>::GetHandle(Type* slot)
	{
		size_t index = slot - slots;
		BK_ASSERT(index < count);

		uint16 generation = generations[index];

		return (static_cast<uint32>(generation) << 16) | index;
	}

	template<typename Type>
	TPoolIterator<Type> TPool<Type>::begin() const
	{
		return TPoolIterator(*this, 0);
	}

	template<typename Type>
	TPoolIterator<Type> TPool<Type>::end() const
	{
		return TPoolIterator(*this, capacity);
	}

	template<typename Type>
	TPoolIterator<Type>::TPoolIterator(const TPool<Type>& pool, size_t start)
		: pool(pool)
	{
		if (start < pool.capacity)
		{
			index = BitsetFind(pool.alive, true, start, pool.capacity);
		}
		else
		{
			index = SIZE_MAX;
		}
	}

	template<typename Type>
	TPoolIterator<Type>& TPoolIterator<Type>::operator++()
	{
		if (index + 1 < pool.capacity)
		{
			index = BitsetFind(pool.alive, true, index + 1, pool.capacity);
		}
		else
		{
			index = SIZE_MAX;
		}

		return *this;
	}

	template<typename Type>
	Type* TPoolIterator<Type>::operator*() const
	{
		return pool.slots + index;
	}

	template<typename Type>
	bool TPoolIterator<Type>::operator!=(const TPoolIterator& other) const
	{
		return index != other.index;
	}
}
