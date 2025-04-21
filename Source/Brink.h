#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <type_traits>

#define BK_ABS(x) ((x) < 0 ? -(x) : (x))
#define BK_MIN(a, b) ((a) < (b) ? (a) : (b))
#define BK_MAX(a, b) ((a) > (b) ? (a) : (b))
#define BK_CLAMP(x, min, max) (((x) > (max)) ? (max) : ((x) < (min)) ? (min) : (x))

#define BK_ARRAY_COUNT(x) (sizeof(x) / sizeof((x)[0]))

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

	template<typename EnumType>
	bool EnumHasAllFlags(EnumType value, EnumType flags)
	{
		using UnderlyingType = std::underlying_type_t<EnumType>;
		return (static_cast<UnderlyingType>(value) & static_cast<UnderlyingType>(flags)) == static_cast<UnderlyingType>(flags);
	}

	template<typename EnumType>
	bool EnumHasAnyFlags(EnumType value, EnumType flags)
	{
		using UnderlyingType = std::underlying_type_t<EnumType>;
		return (static_cast<UnderlyingType>(value) & static_cast<UnderlyingType>(flags)) != 0;
	}

	[[noreturn]] void FatalError(int32 exitCode, const char* format, ...);

	// String

	int32 StringPrintf(char* dst, size_t dstLength, const char* format, ...);
	int32 StringPrintv(char* dst, size_t dstLength, const char* format, va_list args);

	struct String
	{
		String() = default;

		constexpr String(const char* string, size_t length);

		template<size_t N>
		constexpr String(const char (&string)[N]);

		String Slice(size_t offset, size_t count = SIZE_MAX) const;

		char operator[](size_t index) const;

		const char* begin() const;
		const char* end() const;

		const char* data;
		size_t length;
	};

	// Memory

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

	template<typename Type>
	struct Span
	{
		Span() = default;

		Span(Type* data, size_t length);
		Span(std::initializer_list<Type> data);

		template<size_t N>
		Span(Type (&data)[N]);

		Span Slice(size_t offset, size_t count = SIZE_MAX) const;

		Type& operator[](size_t index) const;

		Type* begin() const;
		Type* end() const;

		Type* data;
		size_t length;
	};

	template<typename Type>
	struct Pool
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

	// Application

	struct AppDesc
	{
		void (*initialize)(void);
		void (*update)(void);
	};

	extern AppDesc Main(int32 argc, char** argv);

	// Graphics

	enum class GpuVertexFormat : uint8
	{
		// #TODO: WGPUVertexFormat
		Float32,
		Float32x2,
		Float32x3,
		Float32x4,
	};

	struct GpuVertexBufferAttribute
	{
		uint64 offset;
		GpuVertexFormat format;
	};

	struct GpuVertexBufferDesc
	{
		uint64 stride;
		Span<GpuVertexBufferAttribute> attributes;
	};

	enum class GpuIndexFormat : uint8
	{
		Uint16,
		Uint32,
	};

	struct GpuPipelineDesc
	{
		const char* name;

		struct
		{
			const char* code;
			const char* entryPoint;
			Span<GpuVertexBufferDesc> buffers;
		} VS;

		struct
		{
			const char* code;
			const char* entryPoint;
		} PS;

		Span<uint32> bindingLayouts;
		GpuIndexFormat indexFormat;
	};

	enum class GpuBufferType : uint8
	{
		Uniform,
		Storage,
		Vertex,
		Index,
	};

	enum class GpuBufferAccess : uint8
	{
		GpuOnly,
		CpuRead,
		CpuWrite,
	};

	struct GpuBufferDesc
	{
		const char* name;

		GpuBufferType type;
		GpuBufferAccess access;
		uint64 size;

		Span<uint8> data;
	};

	enum class GpuBindingType : uint8
	{
		None,
		UniformBuffer,
		StorageBuffer,
		DynamicUniformBuffer,
		DynamicStorageBuffer,
		// Texture,
		// Sampler,
	};

	enum class GpuBindingStage : uint8
	{
		None,
		Vertex = (1 << 0),
		Pixel = (1 << 1),
		Compute = (1 << 2),
		All = (Vertex | Pixel | Compute),
	};

	struct GpuBindingLayoutEntry
	{
		GpuBindingType type;
		GpuBindingStage stage;
	};

	struct GpuBindingLayoutDesc
	{
		const char* name;
		Span<GpuBindingLayoutEntry> bindings;
	};

	struct GpuBindingGroupEntry
	{
		uint32 buffer;
		uint64 bufferOffset;
		// uint32 texture;
		// uint32 sampler;
	};

	struct GpuBindingGroupDesc
	{
		const char* name;
		uint32 bindingLayout;
		Span<GpuBindingGroupEntry> bindings;
	};

	struct GpuPassDesc
	{
		const char* name;
		float clearColor[4];
	};

	struct GpuDrawDesc
	{
		uint32 pipeline;
		uint32 vertexBuffer;
		uint32 indexBuffer;
		Span<uint32> bindingGroups;

		uint32 vertexOffset;
		uint32 indexOffset;
		uint32 instanceOffset;

		uint32 triangleCount;
		uint32 instanceCount;
	};

	uint32 CreatePipeline(const GpuPipelineDesc& desc);
	void DestroyPipeline(uint32 handle);

	uint32 CreateBuffer(const GpuBufferDesc& desc);
	void DestroyBuffer(uint32 handle);

	uint32 CreateBindingLayout(const GpuBindingLayoutDesc& desc);
	void DestroyBindingLayout(uint32 handle);

	uint32 CreateBindingGroup(const GpuBindingGroupDesc& desc);
	void DestroyBindingGroup(uint32 handle);

	void BeginPass(const GpuPassDesc& desc);
	void EndPass();

	void Draw(const GpuDrawDesc& desc);
} // namespace Bk

// =========================================================================================================================

namespace Bk
{
	template<typename T>
	constexpr T AlignUp(T value, uintptr_t alignment)
	{
		return T(((uintptr_t)value + alignment - 1) & ~(alignment - 1));
	}

	constexpr String::String(const char* string, size_t length)
		: data(string), length(length)
	{
	}

	template<size_t N>
	constexpr String::String(const char (&string)[N])
		: data(string), length(N - 1)
	{
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

	template<typename Type>
	Span<Type>::Span(Type* data, size_t length)
		: data(data), length(length)
	{
	}

	template<typename Type>
	Span<Type>::Span(std::initializer_list<Type> data)
		: data(const_cast<Type*>(data.begin())), length(data.size())
	{
	}

	template<typename Type>
	template<size_t N>
	Span<Type>::Span(Type (&data)[N])
		: data(data), length(N)
	{
	}

	template<typename Type>
	Span<Type> Span<Type>::Slice(size_t offset, size_t count) const
	{
		BK_ASSERT(offset >= 0 && offset < length);
		return Span(data + offset, BK_MIN(count, length - offset));
	}

	template<typename Type>
	Type& Span<Type>::operator[](size_t index) const
	{
		BK_ASSERT(index >= 0 && index < length);
		return data[index];
	}

	template<typename Type>
	Type* Span<Type>::begin() const
	{
		return data;
	}

	template<typename Type>
	Type* Span<Type>::end() const
	{
		return data + length;
	}

	template<typename Type>
	void Pool<Type>::Initialize(Arena* arena, uint16 size)
	{
		capacity = size;
		count = 0;

		items = arena->Push<Type>(capacity);
		generations = arena->PushZeroed<uint16>(capacity);
		alive = arena->PushZeroed<uint32>((capacity + 31) / 32);
		nextFree = 0;
	}

	template<typename Type>
	Type* Pool<Type>::AllocateItem(uint32* handle)
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
	void Pool<Type>::FreeItem(uint32 handle)
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
	uint32 Pool<Type>::GetHandle(Type* item)
	{
		size_t index = item - items;
		BK_ASSERT(index < count);

		uint16 generation = generations[index];

		return (static_cast<uint32>(generation) << 16) | index;
	}

	template<typename Type>
	Type* Pool<Type>::GetItem(uint32 handle)
	{
		size_t index = handle & 0xFFFF;
		BK_ASSERT(index < count);

		uint16 generation = handle >> 16;
		BK_ASSERT(generation == generations[index]);

		return items + index;
	}
} // namespace Bk
