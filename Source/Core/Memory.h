#pragma once

#include "Core.h"
#include "Span.h"

#define BK_KILOBYTES(x) ((x) << 10)
#define BK_MEGABYTES(x) ((x) << 20)
#define BK_GIGABYTES(x) ((x) << 30)
#define BK_TERABYTES(x) ((x) << 40)

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment);

	void* MemoryAllocate(size_t size);
	void MemoryDeallocate(void* ptr, size_t size);

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
	size_t BitsetFind(const uint32* bitset, bool value, size_t offset, size_t length);

	enum class ArenaFlags : uint8
	{
		KeepFirstBlock = (1 << 0),
	};

	BK_ENUM_FLAGS(ArenaFlags);

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
		ArenaFlags flags;
	};

	struct ArenaScope
	{
		ArenaScope(Arena& arena);
		~ArenaScope();

		Arena& arena;
		ArenaMarker marker;
	};

	ArenaScope GetScratchArena(Arena* persistentArena = nullptr);
}

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment)
	{
		return T(((uintptr_t)value + alignment - 1) & ~(alignment - 1));
	}

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
