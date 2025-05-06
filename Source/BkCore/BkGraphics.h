#pragma once

#include "BkCore.h"
#include "BkSpan.h"

namespace Bk
{
	enum class GfxVertexFormat : uint8
	{
		// #TODO: WGPUVertexFormat
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

	struct GfxPipelineDesc
	{
		const char* name;

		struct
		{
			const char* code;
			const char* entryPoint;
			TSpan<GfxVertexBufferDesc> buffers;
		} VS;

		struct
		{
			const char* code;
			const char* entryPoint;
		} PS;

		TSpan<uint32> bindingLayouts;
		GfxIndexFormat indexFormat;
	};

	enum class GfxBufferType : uint8
	{
		Uniform,
		Storage,
		Vertex,
		Index,
	};

	enum class GfxBufferAccess : uint8
	{
		GpuOnly,
		CpuRead,
		CpuWrite,
	};

	struct GfxBufferDesc
	{
		const char* name;

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
		DynamicUniformBuffer,
		DynamicStorageBuffer,
		// Texture,
		// Sampler,
	};

	enum class GfxBindingStage : uint8
	{
		None,
		Vertex = (1 << 0),
		Pixel = (1 << 1),
		Compute = (1 << 2),
		All = (Vertex | Pixel | Compute),
	};

	BK_ENUM_CLASS_FLAGS(GfxBindingStage);

	struct GfxBindingLayoutEntry
	{
		GfxBindingType type;
		GfxBindingStage stage;
	};

	struct GfxBindingLayoutDesc
	{
		const char* name;
		TSpan<GfxBindingLayoutEntry> bindings;
	};

	struct GfxBindingGroupEntry
	{
		uint32 buffer;
		uint64 bufferOffset;
		// uint32 texture;
		// uint32 sampler;
	};

	struct GfxBindingGroupDesc
	{
		const char* name;
		uint32 bindingLayout;
		TSpan<GfxBindingGroupEntry> bindings;
	};

	struct GfxPassDesc
	{
		const char* name;
		float clearColor[4];
	};

	struct GfxDrawDesc
	{
		uint32 pipeline;
		uint32 vertexBuffer;
		uint32 indexBuffer;
		TSpan<uint32> bindingGroups;

		uint32 vertexOffset;
		uint32 indexOffset;
		uint32 instanceOffset;

		uint32 triangleCount;
		uint32 instanceCount;
	};

    void GfxInitialize();

	uint32 CreatePipeline(const GfxPipelineDesc& desc);
	void DestroyPipeline(uint32 handle);

	uint32 CreateBuffer(const GfxBufferDesc& desc);
	void DestroyBuffer(uint32 handle);

	uint32 CreateBindingLayout(const GfxBindingLayoutDesc& desc);
	void DestroyBindingLayout(uint32 handle);

	uint32 CreateBindingGroup(const GfxBindingGroupDesc& desc);
	void DestroyBindingGroup(uint32 handle);

	bool BeginFrame();
	bool EndFrame();

	void BeginPass(const GfxPassDesc& desc);
	void EndPass();

	void Draw(const GfxDrawDesc& desc);
}
