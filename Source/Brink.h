#pragma once

#include <stddef.h>
#include <stdint.h>

#define BK_ABS(x) ((x) < 0 ? -(x) : (x))
#define BK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BK_CLAMP(x, min, max) (((x) > (max)) ? (max) : ((x) < (min)) ? (min) : (x))

#define BK_ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))
#define BK_ALIGN(x, alignment) (((x) + ((alignment) - 1)) & ~((alignment) - 1))

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

	template<typename ItemType>
	struct Pool
	{
		static_assert(sizeof(ItemType) >= sizeof(uintptr_t), "Pool item type must be at least the size of a pointer");

		void Initialize(Arena* arena, uint16 size);

		ItemType* AllocateItem(uint32* handle = nullptr);
		void FreeItem(uint32 handle);

		uint32 GetHandle(ItemType* item);
		ItemType* GetItem(uint32 handle);

		ItemType* items;
		uint16* generations;
		uint32* alive;
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
} // namespace Bk

// =========================================================================================================================

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

	template<typename ItemType>
	void Pool<ItemType>::Initialize(Arena* arena, uint16 size)
	{
		capacity = size;
		count = 0;

		items = arena->Push<ItemType>(capacity);
		generations = arena->PushZeroed<uint16>(capacity);
		alive = arena->PushZeroed<uint32>((capacity + 31) / 32);
		nextFree = 0;
	}

	template<typename ItemType>
	ItemType* Pool<ItemType>::AllocateItem(uint32* handle)
	{
		ItemType* item;
		size_t index;

		if (nextFree)
		{
			item = reinterpret_cast<ItemType*>(nextFree);
			index = item - items;

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
				*handle = (generation << 16) | index;
			}
		}

		return item;
	}

	template<typename ItemType>
	void Pool<ItemType>::FreeItem(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		generations[index] += 1;
		BitsetUnset(alive, index);

		ItemType* item = items + index;
		*reinterpret_cast<uintptr_t*>(item) = nextFree;
		nextFree = reinterpret_cast<uintptr_t>(item);
	}

	template<typename ItemType>
	uint32 Pool<ItemType>::GetHandle(ItemType* item)
	{
		size_t index = item - items;
		BK_ASSERT(index < count);

		uint16 generation = generations[index];

		return (generation << 16) | index;
	}

	template<typename ItemType>
	ItemType* Pool<ItemType>::GetItem(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		return items + index;
	}
} // namespace Bk