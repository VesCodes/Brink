#pragma once

#include "BkCore/BkCore.h"
#include "BkCore/BkSpan.h"

namespace Bk
{
	struct AppDesc
	{
		void (*initialize)(void);
		void (*update)(void);
	};

	extern AppDesc Main(int32 argc, char** argv);

	// #TODO: Move to BkCore

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
		TSpan<GpuVertexBufferAttribute> attributes;
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
			TSpan<GpuVertexBufferDesc> buffers;
		} VS;

		struct
		{
			const char* code;
			const char* entryPoint;
		} PS;

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
		const char* name;

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
		DynamicUniformBuffer,
		DynamicStorageBuffer,
		// Texture,
		// Sampler,
	};

	// #TODO: Should be flags
	enum class GpuBindingStage : uint8
	{
		None,
		Vertex,
		Pixel,
		Compute,
		All,
	};

	struct GpuBindingLayoutEntry
	{
		GpuBindingType type;
		GpuBindingStage stage;
	};

	struct GpuBindingLayoutDesc
	{
		const char* name;
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
		const char* name;
		uint32 bindingLayout;
		TSpan<GpuBindingGroupEntry> bindings;
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
		TSpan<uint32> bindingGroups;

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
}
