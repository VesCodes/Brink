#pragma once

#include "BkCore.h"

#define BK_KILOBYTES(x) ((x) << 10)
#define BK_MEGABYTES(x) ((x) << 20)
#define BK_GIGABYTES(x) ((x) << 30)
#define BK_TERABYTES(x) ((x) << 40)

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment);

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
}

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment)
	{
		return T(((uintptr_t)value + alignment - 1) & ~(alignment - 1));
	}

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
