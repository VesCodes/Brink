#pragma once

#include "Core/Core.h"
#include "Core/Span.h"
#include "Core/String.h"

namespace Bk
{
	enum class GpuVertexFormat : uint8
	{
		// #TODO: WGPUVertexFormat
		Uint8,
		Uint8x2,
		Uint8x4,
		Uint16,
		Uint16x2,
		Uint16x4,
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
		TSpan<GpuVertexBufferAttribute> attributes;
	};

	enum class GpuIndexFormat : uint8
	{
		Uint16,
		Uint32,
	};

	struct GpuPipelineDesc
	{
		String name;

		struct
		{
			String code;
			String entryPoint;
			TSpan<GpuVertexBufferDesc> buffers;
		} vertexShader;

		struct
		{
			String code;
			String entryPoint;
		} pixelShader;

		TSpan<uint32> bindingLayouts;
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
		String name;

		GpuBufferType type;
		GpuBufferAccess access;
		uint64 size;

		TSpan<uint8> data;
	};

	enum class GpuBindingType : uint8
	{
		None,
		UniformBuffer,
		StorageBuffer,
		ReadOnlyStorageBuffer,
		DynamicUniformBuffer,
		DynamicStorageBuffer,
		DynamicReadOnlyStorageBuffer,
		// Texture,
		// Sampler,
	};

	enum class GpuBindingStage : uint8
	{
		Vertex = (1 << 0),
		Pixel = (1 << 1),
		Compute = (1 << 2),
		All = (Vertex | Pixel | Compute),
	};

	BK_ENUM_FLAGS(GpuBindingStage);

	struct GpuBindingLayoutEntry
	{
		GpuBindingType type;
		GpuBindingStage stage;
	};

	struct GpuBindingLayoutDesc
	{
		String name;
		TSpan<GpuBindingLayoutEntry> bindings;
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
		String name;
		uint32 bindingLayout;
		TSpan<GpuBindingGroupEntry> bindings;
	};

	struct GpuSurfaceDesc
	{
		uint32 width;
		uint32 height;
	};

	struct GpuPassDesc
	{
		String name;
		uint32 surface;
		float clearColor[4];
	};

	struct GpuDrawDesc
	{
		uint32 pipeline;
		TSpan<uint32> vertexBuffers;
		uint32 indexBuffer;
		TSpan<uint32> bindingGroups;

		uint32 vertexOffset;
		uint32 indexOffset;
		uint32 instanceOffset;

		uint32 triangleCount;
		uint32 instanceCount;
	};

	void GpuInitialize();

	uint32 CreatePipeline(const GpuPipelineDesc& desc);
	void DestroyPipeline(uint32 handle);

	uint32 CreateBuffer(const GpuBufferDesc& desc);
	void WriteBuffer(uint32 handle, TSpan<uint8> data, uint64 offset = 0);
	void DestroyBuffer(uint32 handle);

	uint32 CreateBindingLayout(const GpuBindingLayoutDesc& desc);
	void DestroyBindingLayout(uint32 handle);

	uint32 CreateBindingGroup(const GpuBindingGroupDesc& desc);
	void DestroyBindingGroup(uint32 handle);

	uint32 CreateSurface(void* target, const GpuSurfaceDesc& desc);
	void ConfigureSurface(uint32 handle, const GpuSurfaceDesc& desc);
	void PresentSurface(uint32 handle);
	void DestroySurface(uint32 handle);

	bool BeginFrame();
	bool EndFrame();

	void BeginPass(const GpuPassDesc& desc);
	void EndPass();

	void Draw(const GpuDrawDesc& desc);
}
