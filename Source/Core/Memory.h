#pragma once

#include "Core.h"
#include "Span.h"
#include "String.h"

#define BK_KILOBYTES(x) ((x) << 10)
#define BK_MEGABYTES(x) ((x) << 20)
#define BK_GIGABYTES(x) ((x) << 30)
#define BK_TERABYTES(x) ((x) << 40)

namespace Bk
{
	template<typename Type>
	constexpr Type AlignUp(Type value, size_t alignment);

	void* AllocateMemory(size_t size);
	void DeallocateMemory(void* ptr, size_t size);

	void* CopyMemory(void* dst, const void* src, size_t size);
	void* MoveMemory(void* dst, const void* src, size_t size);
	int32 CompareMemory(const void* a, const void* b, size_t size);
	void* SetMemory(void* ptr, int32 value, size_t size);
	void* ZeroMemory(void* ptr, size_t size);

	uint32 CountLeadingZeros(uint64 value);
	uint32 CountTrailingZeros(uint64 value);

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

		ArenaBlock* block;
		size_t blockAlignment;
		ArenaFlags flags;
	};

	TSpan<uint8> Push(Arena& arena, size_t size, size_t alignment = Arena::DefaultAlignment);
	TSpan<uint8> PushZeroed(Arena& arena, size_t size, size_t alignment = Arena::DefaultAlignment);

	template<typename Type>
	TSpan<Type> Push(Arena& arena, size_t count = 1);

	template<typename Type>
	TSpan<Type> PushZeroed(Arena& arena, size_t count = 1);

	ArenaMarker PushMarker(const Arena& arena);
	void PopMarker(Arena& arena, ArenaMarker marker);

	template<typename Type>
	TSpan<Type> Copy(Arena& arena, TSpan<Type> source);

	String Copy(Arena& arena, String source, bool nullTerminate = false);

	struct ArenaScope
	{
		ArenaScope(Arena& arena);
		~ArenaScope();

		Arena& arena;
		ArenaMarker marker;
	};

	ArenaScope GetScratchArena(const Arena* persistentArena = nullptr);

	struct BitArray
	{
		uint32* data;
		size_t length;
	};

	void Allocate(Arena& arena, BitArray& array, size_t length);
	void Copy(BitArray& dst, BitArray src);

	bool GetBit(BitArray array, size_t idx);
	void SetBit(BitArray array, size_t idx);
	void ClearBit(BitArray array, size_t idx);
	size_t FindBit(BitArray array, bool value, size_t start = 0);
}

namespace Bk
{
	template<typename Type>
	constexpr Type AlignUp(Type value, size_t alignment)
	{
		return __builtin_align_up(value, alignment);
	}

	template<typename Type>
	TSpan<Type> Push(Arena& arena, size_t count)
	{
		uint8* data = Push(arena, sizeof(Type) * count, alignof(Type));

		TSpan<Type> result = {};
		result.data = reinterpret_cast<Type*>(data);
		result.length = count;

		return result;
	}

	template<typename Type>
	TSpan<Type> PushZeroed(Arena& arena, size_t count)
	{
		uint8* data = PushZeroed(arena, sizeof(Type) * count, alignof(Type));

		TSpan<Type> result = {};
		result.data = reinterpret_cast<Type*>(data);
		result.length = count;

		return result;
	}

	template<typename Type>
	TSpan<Type> Copy(Arena& arena, TSpan<Type> source)
	{
		TSpan<Type> result = Push<Type>(arena, source.length);
		CopyMemory(result.data, source.data, source.length * sizeof(Type));

		return result;
	}
}
