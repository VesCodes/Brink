#pragma once

#include <stddef.h>
#include <stdint.h>

#define BK_ABS(x) ((x) < 0 ? -(x) : (x))
#define BK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BK_CLAMP(x, min, max) (((x) > (max)) ? (max) : ((x) < (min)) ? (min) : (x))

#define BK_ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))
#define BK_ALIGN(addr, alignment) (((addr) + ((alignment) - 1)) & ~((alignment) - 1))

#define BK_KILOBYTES(x) ((x) * 1024)
#define BK_MEGABYTES(x) (BK_KILOBYTES(x) * 1024)
#define BK_GIGABYTES(x) (BK_MEGABYTES(x) * 1024)
#define BK_TERABYTES(x) (BK_GIGABYTES(x) * 1024)

#define BK_BREAK() __builtin_trap()
#define BK_ASSERT(expr) do { if (!(expr)) { BK_BREAK(); } } while (0)

namespace Bk
{
	using int8 = int8_t;
	using int16 = int16_t;
	using int32 = int32_t;
	using int64 = int64_t;

	using uint8 = uint8_t;
	using uint16 = uint16_t;
	using uint32 = uint32_t;
	using uint64 = uint64_t;

	[[noreturn]] void FatalError(int32 exitCode, const char* format, ...);

	// Memory

	void* MemoryReserve(size_t size);
	bool MemoryRelease(void* ptr, size_t size);
	bool MemoryCommit(void* ptr, size_t size);
	bool MemoryDecommit(void* ptr, size_t size);

	void* MemoryCopy(void* dst, const void* src, size_t size);
	void* MemoryMove(void* dst, const void* src, size_t size);
	int32 MemoryCompare(const void* a, const void* b, size_t size);
	void* MemorySet(void* ptr, int32 value, size_t size);
	void* MemoryZero(void* ptr, size_t size);

	uint32 CountLeadingZeros(uint64 value);
	uint32 CountTrailingZeros(uint64 value);

	bool BitsetIsSet(const uint32* bitset, size_t index);
	void BitsetSet(uint32* bitset, size_t index);
	void BitsetUnset(uint32* bitset, size_t index);
	size_t BitsetFindNextSet(const uint32* bitset, size_t bitsetLength, size_t index);
	size_t BitsetFindNextUnset(const uint32* bitset, size_t bitsetLength, size_t index);

	struct Arena
	{
		static constexpr size_t ReserveGranularity = BK_MEGABYTES(1);
		static constexpr size_t CommitGranularity = BK_MEGABYTES(1);
		static constexpr size_t DecommitThreshold = BK_MEGABYTES(4);

		void Initialize(size_t size);
		void Deinitialize();

		uint8* Push(size_t size, size_t alignment);
		uint8* PushZeroed(size_t size, size_t alignment);
		void Pop(size_t size);

		template<typename Type>
		Type* Push(size_t count = 1);

		template<typename Type>
		Type* PushZeroed(size_t count = 1);

		uint8* data;
		size_t reservedSize;
		size_t committedSize;
		size_t position;
	};

	template<typename ItemType, typename HandleType = uint32, size_t GenerationBits = 12>
	struct Pool
	{
		static_assert(sizeof(ItemType) >= sizeof(uintptr_t), "Pool item type must be at least the size of a pointer");
		static_assert(GenerationBits < sizeof(HandleType) * 8, "Pool handle type can't fit generation bits");

		static constexpr size_t GenerationBytes = (GenerationBits + 7) / 8;
		static constexpr HandleType GenerationMask = (HandleType(1) << GenerationBits) - 1;
		static constexpr size_t IndexBits = (sizeof(HandleType) * 8) - GenerationBits;
		static constexpr HandleType IndexMask = (HandleType(1) << IndexBits) - 1;

		void Initialize(Arena* arena, size_t inCapacity);

		ItemType* AllocateItem(HandleType* handle = nullptr);
		void FreeItem(HandleType handle);

		HandleType GetHandle(ItemType* item);
		HandleType GetHandleFromIndex(size_t index);

		ItemType* GetItem(HandleType handle);
		ItemType* GetItemFromIndex(size_t index);

		struct Iterator
		{
			Iterator() = default;

			Iterator(Pool& pool, size_t index = 0)
				: pool(pool), index(index)
			{
				index = BitsetFindNextSet(pool.aliveBitset, pool.count, index);
				if (index == -1)
				{
					index = pool.count;
				}
			}

			bool operator!=(const Iterator& other) const
			{
				return index != other.index;
			}

			Iterator& operator++()
			{
				index = BitsetFindNextSet(pool.aliveBitset, pool.count, index + 1);
				if (index == -1)
				{
					index = pool.count;
				}

				return *this;
			}

			ItemType* operator*() const
			{
				return pool->GetItemFromIndex(index);
			}

			Pool& pool;
			size_t index;
		};

		ItemType* items;
		uint8* generations;
		uint32* aliveBitset;
		uintptr_t nextFree;

		size_t capacity;
		size_t count;
	};

	// Graphics

	enum class ShaderType : uint8
	{
		Vertex,
		Pixel,
	};

	struct ShaderDesc
	{
		ShaderType type;
		const char* entrypoint;
		const char* code;
	};

	struct PipelineDesc
	{
		uint32 vertexShader;
		uint32 pixelShader;
	};

	uint32 CreateShader(const ShaderDesc& desc);
	uint32 CreatePipeline(const PipelineDesc& desc);

	//
	//
	//

	template<typename Type>
	inline Type* Arena::Push(size_t count)
	{
		return (Type*)Push(sizeof(Type) * count, alignof(Type));
	}

	template<typename Type>
	inline Type* Arena::PushZeroed(size_t count)
	{
		return (Type*)PushZeroed(sizeof(Type) * count, alignof(Type));
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline void Pool<ItemType, HandleType, GenerationBits>::Initialize(Arena* arena, size_t inCapacity)
	{
		// Can't fit more items than max index
		BK_ASSERT(inCapacity <= IndexMask);

		items = arena->Push<ItemType>(inCapacity);
		generations = arena->Push(inCapacity * GenerationBytes, alignof(HandleType));
		aliveBitset = arena->Push<uint32>((inCapacity + 31) / 32);
		nextFree = 0;

		capacity = inCapacity;
		count = 0;
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline ItemType* Pool<ItemType, HandleType, GenerationBits>::AllocateItem(HandleType* handle)
	{
		ItemType* item;
		size_t index;

		if (nextFree)
		{
			item = (ItemType*)(nextFree);
			index = (size_t)(item - items);

			nextFree = *(uintptr_t*)(nextFree);
		}
		else if (count < capacity)
		{
			item = items + count;
			index = count;

			count += 1;
		}

		if (item)
		{
			// #TODO: Sanity check generation logic
			HandleType* generationPtr = (HandleType*)(generations + (index * GenerationBytes));

			HandleType generation = (*generationPtr + 1) & GenerationMask;
			if (generation == 0)
			{
				// #TODO: HandleType generation wrapping
				generation = 1;
			}

			*generationPtr |= (generation & GenerationMask);
			BitsetSet(aliveBitset, index);

			if (handle)
			{
				*handle = (generation << IndexBits) | index;
			}
		}

		return item;
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline void Pool<ItemType, HandleType, GenerationBits>::FreeItem(HandleType handle)
	{
		size_t index = handle & IndexMask;
		BK_ASSERT(index < count);

		HandleType generation = *(HandleType*)(generations + (index * GenerationBytes)) & GenerationMask;
		HandleType handleGeneration = handle >> IndexBits;
		BK_ASSERT(generation == handleGeneration);

		bool isAlive = BitsetIsSet(aliveBitset, index);
		BK_ASSERT(isAlive);

		BitsetUnset(aliveBitset, index);

		ItemType* item = items + index;
		*(uintptr_t*)(item) = nextFree;
		nextFree = (uintptr_t)(item);
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline HandleType Pool<ItemType, HandleType, GenerationBits>::GetHandle(ItemType* item)
	{
		size_t index = (size_t)(item - items);
		return GetHandleFromIndex(index);
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline HandleType Pool<ItemType, HandleType, GenerationBits>::GetHandleFromIndex(size_t index)
	{
		BK_ASSERT(index < count);

		HandleType generation = *(HandleType*)(generations + (index * GenerationBytes)) & GenerationMask;
		HandleType handle = (generation << IndexBits) | index;

		return handle;
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline ItemType* Pool<ItemType, HandleType, GenerationBits>::GetItem(HandleType handle)
	{
		size_t index = handle & IndexMask;
		BK_ASSERT(index < count);

		HandleType generation = *(HandleType*)(generations + (index * GenerationBytes)) & GenerationMask;
		HandleType handleGeneration = handle >> IndexBits;
		BK_ASSERT(generation == handleGeneration);

		return GetItemFromIndex(index);
	}

	template<typename ItemType, typename HandleType, size_t GenerationBits>
	inline ItemType* Pool<ItemType, HandleType, GenerationBits>::GetItemFromIndex(size_t index)
	{
		BK_ASSERT(index < count);

		bool isAlive = BitsetIsSet(aliveBitset, index);
		BK_ASSERT(isAlive);

		return items + index;
	}
} // namespace Bk
