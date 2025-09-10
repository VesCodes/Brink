#pragma once

#include "Core/Core.h"
#include "Core/Span.h"
#include "Core/String.h"

namespace Bk
{
	enum class GfxVertexFormat : uint8
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

	struct GfxVertexBufferAttribute
	{
		uint64 offset;
		GfxVertexFormat format;
	};

	struct GfxVertexBufferDesc
	{
		uint64 stride;
		TSpan<GfxVertexBufferAttribute> attributes;
	};

	enum class GfxIndexFormat : uint8
	{
		Uint16,
		Uint32,
	};

	struct GfxRenderPipelineDesc
	{
		String name;

		struct
		{
			String code;
			String entryPoint;
			TSpan<GfxVertexBufferDesc> buffers;
		} vertexShader;

		struct
		{
			String code;
			String entryPoint;
		} pixelShader;

		TSpan<uint32> bindingLayouts;
		GfxIndexFormat indexFormat;
	};

	struct GfxComputePipelineDesc
	{
		String name;

		struct
		{
			String code;
			String entryPoint;
		} computeShader;

		TSpan<uint32> bindingLayouts;
	};

	enum class GfxBufferType : uint8
	{
		Uniform = (1 << 0),
		Storage = (1 << 1),
		Vertex = (1 << 2),
		Index = (1 << 3),
	};

	BK_ENUM_FLAGS(GfxBufferType);

	enum class GfxBufferAccess : uint8
	{
		GpuOnly,
		CpuRead,
		CpuWrite,
	};

	struct GfxBufferDesc
	{
		String name;

		GfxBufferType type;
		GfxBufferAccess access;
		uint64 size;

		TSpan<uint8> data;
	};

	enum class GfxBindingType : uint8
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

	enum class GfxBindingStage : uint8
	{
		Vertex = (1 << 0),
		Pixel = (1 << 1),
		Compute = (1 << 2),
		All = (Vertex | Pixel | Compute),
	};

	BK_ENUM_FLAGS(GfxBindingStage);

	struct GfxBindingLayoutEntry
	{
		GfxBindingType type;
		GfxBindingStage stage;
	};

	struct GfxBindingLayoutDesc
	{
		String name;
		TSpan<GfxBindingLayoutEntry> bindings;
	};

	struct GfxBindingGroupEntry
	{
		uint32 buffer;
		uint64 offset;
		uint64 size;
		// uint32 texture;
		// uint32 sampler;
	};

	struct GfxBindingGroupDesc
	{
		String name;
		uint32 bindingLayout;
		TSpan<GfxBindingGroupEntry> bindings;
	};

	struct GfxSurfaceDesc
	{
		uint32 width;
		uint32 height;
	};

	struct GfxRenderPassDesc
	{
		String name;
		uint32 surface;
		float clearColor[4];
	};

	struct GfxComputePassDesc
	{
		String name;
	};

	struct GfxBindingGroupOffsets
	{
		uint32 bindingGroup;
		TSpan<uint32> dynamicOffsets;
	};

	struct GfxDrawDesc
	{
		uint32 pipeline;
		TSpan<GfxBindingGroupOffsets> bindingGroups;

		TSpan<uint32> vertexBuffers;
		uint32 indexBuffer;

		uint32 vertexOffset;
		uint32 indexOffset;
		uint32 instanceOffset;

		uint32 triangleCount;
		uint32 instanceCount;
	};

	struct GfxDispatchDesc
	{
		uint32 pipeline;
		TSpan<GfxBindingGroupOffsets> bindingGroups;

		uint32 workgroupCountX;
		uint32 workgroupCountY;
		uint32 workgroupCountZ;
	};

	void InitializeGraphics();

	uint32 CreateRenderPipeline(const GfxRenderPipelineDesc& desc);
	void DestroyRenderPipeline(uint32 handle);

	uint32 CreateComputePipeline(const GfxComputePipelineDesc& desc);
	void DestroyComputePipeline(uint32 handle);

	uint32 CreateBuffer(const GfxBufferDesc& desc);
	size_t WriteBuffer(uint32 handle, TSpan<const uint8> data, uint64 offset = 0);
	void DestroyBuffer(uint32 handle);

	uint32 CreateBindingLayout(const GfxBindingLayoutDesc& desc);
	void DestroyBindingLayout(uint32 handle);

	uint32 CreateBindingGroup(const GfxBindingGroupDesc& desc);
	void DestroyBindingGroup(uint32 handle);

	uint32 CreateSurface(void* target, const GfxSurfaceDesc& desc);
	void ConfigureSurface(uint32 handle, const GfxSurfaceDesc& desc);
	void PresentSurface(uint32 handle);
	void DestroySurface(uint32 handle);

	bool BeginFrame();
	bool EndFrame();

	void BeginRenderPass(const GfxRenderPassDesc& desc);
	void EndRenderPass();

	void BeginComputePass(const GfxComputePassDesc& desc);
	void EndComputePass();

	void Draw(const GfxDrawDesc& desc);
	void Dispatch(const GfxDispatchDesc& desc);
}
