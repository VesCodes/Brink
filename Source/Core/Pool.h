#pragma once

#include "Core.h"
#include "Memory.h"

namespace Bk
{
	template<typename Type>
	struct TPool
	{
		static_assert(sizeof(Type) >= sizeof(uintptr_t), "Pool type must be at least the size of a pointer");

		Type* slots;
		uint16* generations;
		uint32* alive;
		uintptr_t nextFree;
		size_t capacity;
		size_t count;
	};

	template<typename Type>
	void Allocate(Arena& arena, TPool<Type>& pool, uint16 capacity);

	template<typename Type>
	Type* AcquireSlot(TPool<Type>& pool, uint32* handle = nullptr);

	template<typename Type>
	void ReleaseSlot(TPool<Type>& pool, uint32 handle);

	template<typename Type>
	Type* GetSlot(TPool<Type>& pool, uint32 handle);

	template<typename Type>
	uint32 GetHandle(TPool<Type>& pool, Type* slot);

	template<typename Type>
	struct TPoolIterator
	{
		TPoolIterator(const TPool<Type>& pool, size_t start);

		TPoolIterator& operator++();
		Type* operator*() const;
		bool operator!=(const TPoolIterator& other) const;

		const TPool<Type>& pool;
		size_t idx;
	};

	template<typename Type>
	TPoolIterator<Type> begin(const TPool<Type>& pool);

	template<typename Type>
	TPoolIterator<Type> end(const TPool<Type>& pool);
}

namespace Bk
{
	template<typename Type>
	void Allocate(Arena& arena, TPool<Type>& pool, uint16 capacity)
	{
		pool.capacity = capacity;
		pool.count = 0;

		pool.slots = Push<Type>(arena, capacity);
		pool.generations = PushZeroed<uint16>(arena, capacity);
		pool.alive = PushZeroed<uint32>(arena, (capacity + 31) / 32);
		pool.nextFree = 0;
	}

	template<typename Type>
	Type* AcquireSlot(TPool<Type>& pool, uint32* handle)
	{
		Type* slot = nullptr;
		size_t idx;

		if (pool.nextFree)
		{
			slot = reinterpret_cast<Type*>(pool.nextFree);
			idx = static_cast<size_t>(slot - pool.slots);

			pool.nextFree = *reinterpret_cast<uintptr_t*>(pool.nextFree);
		}
		else if (pool.count < pool.capacity)
		{
			slot = pool.slots + pool.count;
			idx = pool.count;

			pool.count += 1;
		}

		if (slot)
		{
			uint16& generation = pool.generations[idx];
			if (generation == 0)
			{
				generation = 1;
			}

			BitsetSet(pool.alive, idx);

			if (handle)
			{
				*handle = (static_cast<uint32>(generation) << 16) | idx;
			}
		}

		return slot;
	}

	template<typename Type>
	void ReleaseSlot(TPool<Type>& pool, uint32 handle)
	{
		size_t idx = handle & 0xFFFF;
		BK_ASSERT(idx < pool.count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == pool.generations[idx]);

		pool.generations[idx] += 1;
		BitsetUnset(pool.alive, idx);

		Type* slot = pool.slots + idx;
		*reinterpret_cast<uintptr_t*>(slot) = pool.nextFree;
		pool.nextFree = reinterpret_cast<uintptr_t>(slot);
	}

	template<typename Type>
	Type* GetSlot(TPool<Type>& pool, uint32 handle)
	{
		size_t idx = handle & 0xFFFF;
		BK_ASSERT(idx < pool.count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == pool.generations[idx]);

		return pool.slots + idx;
	}

	template<typename Type>
	uint32 GetHandle(TPool<Type>& pool, Type* slot)
	{
		size_t idx = slot - pool.slots;
		BK_ASSERT(idx < pool.count);

		uint16 generation = pool.generations[idx];

		return (static_cast<uint32>(generation) << 16) | idx;
	}

	template<typename Type>
	TPoolIterator<Type> begin(const TPool<Type>& pool)
	{
		return TPoolIterator(pool, 0);
	}

	template<typename Type>
	TPoolIterator<Type> end(const TPool<Type>& pool)
	{
		return TPoolIterator(pool, pool.capacity);
	}

	template<typename Type>
	TPoolIterator<Type>::TPoolIterator(const TPool<Type>& pool, size_t start)
		: pool(pool)
	{
		if (start < pool.capacity)
		{
			idx = BitsetFind(pool.alive, true, start, pool.capacity);
		}
		else
		{
			idx = SIZE_MAX;
		}
	}

	template<typename Type>
	TPoolIterator<Type>& TPoolIterator<Type>::operator++()
	{
		if (idx + 1 < pool.capacity)
		{
			idx = BitsetFind(pool.alive, true, idx + 1, pool.capacity);
		}
		else
		{
			idx = SIZE_MAX;
		}

		return *this;
	}

	template<typename Type>
	Type* TPoolIterator<Type>::operator*() const
	{
		return pool.slots + idx;
	}

	template<typename Type>
	bool TPoolIterator<Type>::operator!=(const TPoolIterator& other) const
	{
		return idx != other.idx;
	}
}
