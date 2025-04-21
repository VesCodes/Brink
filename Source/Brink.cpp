#include "Brink.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <webgpu/webgpu.h>

#include <stdio.h>
#include <string.h>

namespace Bk
{
	struct AppContext
	{
		Arena arena;
		AppDesc desc;
	} appContext;

	struct GpuPipeline
	{
		WGPURenderPipeline handle;
		WGPUIndexFormat indexFormat;
	};

	struct GpuBuffer
	{
		WGPUBuffer handle;
		uint64 size;
	};

	struct GpuBindGroupLayout
	{
		WGPUBindGroupLayout handle;
	};

	struct GpuContext
	{
		WGPUInstance instance;
		WGPUSurface surface;
		WGPUSurfaceConfiguration surfaceConfig;
		WGPUAdapter adapter;
		WGPUDevice device;
		WGPUQueue queue;

		WGPUCommandEncoder commandEncoder;
		WGPURenderPassEncoder renderPassEncoder;

		Pool<GpuPipeline> pipelines;
		Pool<GpuBuffer> buffers;
		Pool<GpuBindGroupLayout> bindGroupLayouts;
	} gpuContext;

	void FatalError(int32 exitCode, const char* format, ...)
	{
		// #TODO: Error message
		exit(exitCode);
	}

	void* MemoryCopy(void* dst, const void* src, size_t size)
	{
		return memcpy(dst, src, size);
	}

	void* MemoryMove(void* dst, const void* src, size_t size)
	{
		return memmove(dst, src, size);
	}

	int32 MemoryCompare(const void* a, const void* b, size_t size)
	{
		return memcmp(a, b, size);
	}

	void* MemorySet(void* ptr, int32 value, size_t size)
	{
		return memset(ptr, value, size);
	}

	void* MemoryZero(void* ptr, size_t size)
	{
		return memset(ptr, 0, size);
	}

	void* MemoryReserve(size_t size)
	{
		return malloc(size);
	}

	bool MemoryRelease(void* ptr, size_t size)
	{
		free(ptr);
		return true;
	}

	bool MemoryCommit(void* ptr, size_t size)
	{
		// #TODO: Check with malloc_usable_size?
		return true;
	}

	bool MemoryDecommit(void* ptr, size_t size)
	{
		return true;
	}

	uint32 CountLeadingZeros(uint64 value)
	{
		return value ? __builtin_clzll(value) : 64;
	}

	uint32 CountTrailingZeros(uint64 value)
	{
		return value ? __builtin_ctzll(value) : 64;
	}

	bool BitsetIsSet(const uint32* bitset, size_t index)
	{
		size_t wordIdx = index / 32;
		return bitset[wordIdx] & (1u << (index & 31));
	}

	void BitsetSet(uint32* bitset, size_t index)
	{
		size_t wordIdx = index / 32;
		bitset[wordIdx] |= (1u << (index & 31));
	}

	void BitsetUnset(uint32* bitset, size_t index)
	{
		size_t wordIdx = index / 32;
		bitset[wordIdx] &= ~(1u << (index & 31));
	}

	size_t BitsetFindNextSet(const uint32* bitset, size_t bitsetLength, size_t index)
	{
		size_t wordCount = (bitsetLength + 31) / 32;
		size_t wordIdx = index / 32;

		// Mask out bits before index in first word
		uint32 word = bitset[wordIdx] & (UINT32_MAX << (index & 31));

		for (; wordIdx < wordCount; ++wordIdx, word = bitset[wordIdx])
		{
			if (word)
			{
				return wordIdx * 32 + CountTrailingZeros(word);
			}
		}

		return -1;
	}

	size_t BitsetFindNextUnset(const uint32* bitset, size_t bitsetLength, size_t index)
	{
		size_t wordCount = (bitsetLength + 31) / 32;
		size_t wordIdx = index / 32;

		// Mask out bits before index in first word
		uint32 word = ~bitset[wordIdx] & (UINT32_MAX << (index & 31));

		for (; wordIdx < wordCount; ++wordIdx, word = ~bitset[wordIdx])
		{
			if (word)
			{
				return wordIdx * 32 + CountTrailingZeros(word);
			}
		}

		return -1;
	}

	void Arena::Initialize(size_t size)
	{
		const size_t bytesToReserve = BK_ALIGN(size, ReserveGranularity);

		data = static_cast<uint8*>(MemoryReserve(bytesToReserve));
		if (!data)
		{
			FatalError(1, "Failed to reserve %zd bytes for arena", bytesToReserve);
		}

		reservedSize = bytesToReserve;
		committedSize = 0;
		position = 0;
	}

	void Arena::Deinitialize()
	{
		MemoryRelease(data, reservedSize);

		data = nullptr;
		reservedSize = 0;
		committedSize = 0;
		position = 0;
	}

	uint8* Arena::Push(size_t size, size_t alignment)
	{
		const size_t alignedPosition = BK_ALIGN(position, alignment);
		const size_t nextPosition = alignedPosition + size;

		if (nextPosition > committedSize)
		{
			if (nextPosition > reservedSize)
			{
				FatalError(1, "Failed to commit %zd bytes for arena, requested %zd bytes more than available", nextPosition - position, nextPosition - reservedSize);
			}

			size_t bytesToCommit = BK_ALIGN(nextPosition - committedSize, CommitGranularity);
			if (committedSize + bytesToCommit > reservedSize)
			{
				bytesToCommit = reservedSize - committedSize;
			}

			BK_ASSERT(MemoryCommit(data + committedSize, bytesToCommit));
			committedSize += bytesToCommit;
		}

		position = nextPosition;
		return data + alignedPosition;
	}

	uint8* Arena::PushZeroed(size_t size, size_t alignment)
	{
		uint8* result = Push(size, alignment);
		MemoryZero(result, size);

		return result;
	}

	void Arena::Pop(size_t size)
	{
		position = (size < position) ? position - size : 0;

		const size_t alignedPosition = BK_ALIGN(position, CommitGranularity);
		if (alignedPosition + DecommitThreshold <= committedSize)
		{
			const size_t bytesToDecommit = committedSize - alignedPosition;

			BK_ASSERT(MemoryDecommit(data + alignedPosition, bytesToDecommit));
			committedSize -= bytesToDecommit;
		}
	}

	WGPUVertexFormat WgpuConvert(const GpuVertexFormat value)
	{
		switch (value)
		{
			case GpuVertexFormat::Float32: return WGPUVertexFormat_Float32;
			case GpuVertexFormat::Float32x2: return WGPUVertexFormat_Float32x2;
			case GpuVertexFormat::Float32x3: return WGPUVertexFormat_Float32x3;
			case GpuVertexFormat::Float32x4: return WGPUVertexFormat_Float32x4;
			default: return WGPUVertexFormat_Force32;
		}
	}

	WGPUIndexFormat WgpuConvert(const GpuIndexFormat value)
	{
		switch (value)
		{
			case GpuIndexFormat::Uint32: return WGPUIndexFormat_Uint32;
			case GpuIndexFormat::Uint16: return WGPUIndexFormat_Uint16;
			default: return WGPUIndexFormat_Force32;
		}
	};

	WGPUShaderStageFlags WgpuConvert(const GpuBindGroupVisibility value)
	{
		WGPUShaderStageFlags result = WGPUShaderStage_None;

		if (EnumHasAllFlags(value, GpuBindGroupVisibility::Vertex))
		{
			result |= WGPUShaderStage_Vertex;
		}

		if (EnumHasAllFlags(value, GpuBindGroupVisibility::Pixel))
		{
			result |= WGPUShaderStage_Fragment;
		}

		if (EnumHasAllFlags(value, GpuBindGroupVisibility::Compute))
		{
			result |= WGPUShaderStage_Compute;
		}

		return result;
	}

	WGPUBufferBindingType WgpuConvert(const GpuBindGroupBufferType value)
	{
		switch (value)
		{
			case GpuBindGroupBufferType::None: return WGPUBufferBindingType_Undefined;
			case GpuBindGroupBufferType::Uniform: return WGPUBufferBindingType_Uniform;
			case GpuBindGroupBufferType::Storage: return WGPUBufferBindingType_Storage;
			case GpuBindGroupBufferType::ReadOnlyStorage: return WGPUBufferBindingType_ReadOnlyStorage;
		}
	}

	uint32 CreatePipeline(const GpuPipelineDesc& desc)
	{
		WGPURenderPipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = desc.name;
		pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = 0xFFFFFFFF;

		if (desc.VS.code)
		{
			WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
			shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
			shaderCodeDesc.code = desc.VS.code;

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderCodeDesc.chain;

			pipelineDesc.vertex.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			pipelineDesc.vertex.entryPoint = desc.VS.entryPoint;

			// #TODO: Temp arena allocations
			WGPUVertexBufferLayout vertexBuffers[8] = {};
			WGPUVertexAttribute vertexAttributes[8][16] = {};

			pipelineDesc.vertex.buffers = vertexBuffers;
			pipelineDesc.vertex.bufferCount = desc.VS.buffers.length;

			for (int32 bufferIdx = 0; bufferIdx < desc.VS.buffers.length; ++bufferIdx)
			{
				const GpuVertexBufferDesc& bufferDesc = desc.VS.buffers[bufferIdx];
				vertexBuffers[bufferIdx].arrayStride = bufferDesc.stride;
				vertexBuffers[bufferIdx].attributes = vertexAttributes[bufferIdx];
				vertexBuffers[bufferIdx].attributeCount = bufferDesc.attributes.length;

				for (int32 attributeIdx = 0; attributeIdx < bufferDesc.attributes.length; ++attributeIdx)
				{
					const GpuVertexBufferAttribute& attributeDesc = bufferDesc.attributes[attributeIdx];
					vertexAttributes[bufferIdx][attributeIdx].format = WgpuConvert(attributeDesc.format);
					vertexAttributes[bufferIdx][attributeIdx].offset = attributeDesc.offset;
					vertexAttributes[bufferIdx][attributeIdx].shaderLocation = attributeIdx;
				}
			}
		}

		if (desc.PS.code)
		{
			WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
			shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
			shaderCodeDesc.code = desc.PS.code;

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderCodeDesc.chain;

			WGPUFragmentState fragmentState = {};
			fragmentState.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			fragmentState.entryPoint = desc.PS.entryPoint;

			WGPUColorTargetState surfaceTarget = { .format = gpuContext.surfaceConfig.format, .writeMask = WGPUColorWriteMask_All };
			fragmentState.targets = &surfaceTarget;
			fragmentState.targetCount = 1;

			pipelineDesc.fragment = &fragmentState;
		}

		if (desc.bindGroups.length > 0)
		{
			WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
			pipelineLayoutDesc.label = desc.name;

			// #TODO: Temp arena allocations
			WGPUBindGroupLayout bindGroupLayouts[8];

			pipelineLayoutDesc.bindGroupLayouts = bindGroupLayouts;
			pipelineLayoutDesc.bindGroupLayoutCount = desc.bindGroups.length;

			for (int32 layoutIdx = 0; layoutIdx < desc.bindGroups.length; ++layoutIdx)
			{
				bindGroupLayouts[layoutIdx] = gpuContext.bindGroupLayouts.GetItem(desc.bindGroups[layoutIdx])->handle;
			}

			pipelineDesc.layout = wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);
		}

		WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

		if (pipelineDesc.vertex.module)
		{
			wgpuShaderModuleRelease(pipelineDesc.vertex.module);
		}

		if (pipelineDesc.fragment && pipelineDesc.fragment->module)
		{
			wgpuShaderModuleRelease(pipelineDesc.fragment->module);
		}

		if (pipelineDesc.layout)
		{
			wgpuPipelineLayoutRelease(pipelineDesc.layout);
		}

		uint32 pipelineHandle = 0;
		if (pipeline)
		{
			GpuPipeline* pipelineWrapper = gpuContext.pipelines.AllocateItem(&pipelineHandle);
			pipelineWrapper->handle = pipeline;
			pipelineWrapper->indexFormat = WgpuConvert(desc.indexFormat);
		}

		return pipelineHandle;
	}

	uint32 CreateBuffer(const GpuBufferDesc& desc)
	{
		// Allow shorthand of not specifying buffer size if passing data
		const uint64 bufferSize = desc.size == 0 ? desc.data.length : desc.size;

		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.label = desc.name;
		bufferDesc.size = BK_ALIGN(bufferSize, 4); // Mapping requires size to be multiple of 4
		bufferDesc.mappedAtCreation = desc.data.length != 0;

		switch (desc.type)
		{
			case GpuBufferType::Uniform: bufferDesc.usage = WGPUBufferUsage_Uniform; break;
			case GpuBufferType::Storage: bufferDesc.usage = WGPUBufferUsage_Storage; break;
			case GpuBufferType::Vertex: bufferDesc.usage = WGPUBufferUsage_Vertex; break;
			case GpuBufferType::Index: bufferDesc.usage = WGPUBufferUsage_Index; break;
			default: break;
		}

		switch (desc.access)
		{
			case GpuBufferAccess::GpuOnly: bufferDesc.usage |= WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst; break;
			case GpuBufferAccess::CpuRead: bufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst; break;
			case GpuBufferAccess::CpuWrite: bufferDesc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc; break;
		}

		WGPUBuffer buffer = wgpuDeviceCreateBuffer(gpuContext.device, &bufferDesc);
		if (buffer && bufferDesc.mappedAtCreation)
		{
			const size_t bufferMappedSize = BK_MIN(bufferSize, desc.data.length);
			void* bufferMappedPtr = wgpuBufferGetMappedRange(buffer, 0, BK_ALIGN(bufferMappedSize, 4));

			MemoryCopy(bufferMappedPtr, desc.data.data, bufferMappedSize);

			wgpuBufferUnmap(buffer);
		}

		uint32 bufferHandle = 0;
		if (buffer)
		{
			GpuBuffer* bufferWrapper = gpuContext.buffers.AllocateItem(&bufferHandle);
			bufferWrapper->handle = buffer;
			bufferWrapper->size = bufferSize;
		}

		return bufferHandle;
	}

	uint32 CreateBindGroupLayout(const GpuBindGroupLayoutDesc& desc)
	{
		WGPUBindGroupLayoutDescriptor layoutDesc = {};
		layoutDesc.label = desc.name;

		// #TODO: Temp arena allocations
		WGPUBindGroupLayoutEntry entries[8] = {};

		layoutDesc.entries = entries;
		layoutDesc.entryCount = desc.entries.length;

		for (int32 entryIdx = 0; entryIdx < desc.entries.length; ++entryIdx)
		{
			const GpuBindGroupLayoutEntry& entry = desc.entries[entryIdx];
			entries[entryIdx].binding = entryIdx;
			entries[entryIdx].visibility = WgpuConvert(entry.visibility);
			entries[entryIdx].buffer.type = WgpuConvert(entry.bufferType);
			entries[entryIdx].buffer.hasDynamicOffset = entry.bufferDynamicOffset;
		}

		WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(gpuContext.device, &layoutDesc);

		uint32 layoutHandle;
		if (layout)
		{
			GpuBindGroupLayout* layoutWrapper = gpuContext.bindGroupLayouts.AllocateItem(&layoutHandle);
			layoutWrapper->handle = layout;
		}

		return layoutHandle;
	}

	void BeginPass(const GpuPassDesc& desc)
	{
		BK_ASSERT(gpuContext.renderPassEncoder == nullptr);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.label = desc.name;

		WGPUSurfaceTexture surfaceTexture;
		wgpuSurfaceGetCurrentTexture(gpuContext.surface, &surfaceTexture);

		WGPURenderPassColorAttachment colorAttachment = {};
		colorAttachment.view = wgpuTextureCreateView(surfaceTexture.texture, nullptr);
		colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
		colorAttachment.loadOp = WGPULoadOp_Clear;
		colorAttachment.storeOp = WGPUStoreOp_Store;
		colorAttachment.clearValue.r = desc.clearColor[0];
		colorAttachment.clearValue.g = desc.clearColor[1];
		colorAttachment.clearValue.b = desc.clearColor[2];
		colorAttachment.clearValue.a = desc.clearColor[3];

		passDesc.colorAttachments = &colorAttachment;
		passDesc.colorAttachmentCount = 1;

		gpuContext.renderPassEncoder = wgpuCommandEncoderBeginRenderPass(gpuContext.commandEncoder, &passDesc);
	}

	void EndPass()
	{
		BK_ASSERT(gpuContext.renderPassEncoder != nullptr);

		wgpuRenderPassEncoderEnd(gpuContext.renderPassEncoder);
		wgpuRenderPassEncoderRelease(gpuContext.renderPassEncoder);
		gpuContext.renderPassEncoder = nullptr;
	}

	void Draw(const GpuDrawDesc& desc)
	{
		BK_ASSERT(gpuContext.renderPassEncoder != nullptr);
		BK_ASSERT(desc.pipeline);

		GpuPipeline* pipeline = gpuContext.pipelines.GetItem(desc.pipeline);
		wgpuRenderPassEncoderSetPipeline(gpuContext.renderPassEncoder, pipeline->handle);

		if (desc.vertexBuffer)
		{
			GpuBuffer* buffer = gpuContext.buffers.GetItem(desc.vertexBuffer);
			wgpuRenderPassEncoderSetVertexBuffer(gpuContext.renderPassEncoder, 0, buffer->handle, 0, buffer->size);
		}

		if (desc.indexBuffer)
		{
			GpuBuffer* buffer = gpuContext.buffers.GetItem(desc.indexBuffer);
			wgpuRenderPassEncoderSetIndexBuffer(gpuContext.renderPassEncoder, buffer->handle, pipeline->indexFormat, 0, buffer->size);

			wgpuRenderPassEncoderDrawIndexed(
				gpuContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.indexOffset, desc.vertexOffset, desc.instanceOffset);
		}
		else
		{
			wgpuRenderPassEncoderDraw(
				gpuContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.vertexOffset, desc.instanceOffset);
		}
	}

	//
	//
	//

	void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userData);
	void OnDeviceAcquired(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userData);
	void OnDeviceError(WGPUErrorType type, const char* message, void* userData);
	bool OnResize(int eventType, const EmscriptenUiEvent* event, void* userData);
	bool OnFrame(double time, void* userData);
	void ConfigureSurface();

	void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userData)
	{
		printf("Adapter acquired: %p\n", adapter);

		gpuContext.adapter = adapter;

		wgpuAdapterRequestDevice(adapter, nullptr, OnDeviceAcquired, nullptr);
	}

	void OnDeviceAcquired(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userData)
	{
		printf("Device acquired: %p\n", device);

		wgpuDeviceSetUncapturedErrorCallback(device, OnDeviceError, nullptr);

		gpuContext.device = device;
		gpuContext.queue = wgpuDeviceGetQueue(gpuContext.device);

		WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc = {};
		canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
		canvasDesc.selector = "#canvas";

		WGPUSurfaceDescriptor surfaceDesc = {};
		surfaceDesc.nextInChain = &canvasDesc.chain;
		gpuContext.surface = wgpuInstanceCreateSurface(gpuContext.instance, &surfaceDesc);

		ConfigureSurface();

		appContext.desc.initialize();
	}

	void OnDeviceError(WGPUErrorType type, const char* message, void* userData)
	{
		printf("Device Error (%0x): %s\n", type, message);
	}

	bool OnResize(int eventType, const EmscriptenUiEvent* event, void* userData)
	{
		ConfigureSurface();

		return true;
	}

	bool OnFrame(double time, void* userData)
	{
		if (!gpuContext.device)
		{
			return true;
		}

		gpuContext.commandEncoder = wgpuDeviceCreateCommandEncoder(gpuContext.device, nullptr);

		appContext.desc.update();

		WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(gpuContext.commandEncoder, nullptr);
		wgpuQueueSubmit(gpuContext.queue, 1, &commandBuffer);
		wgpuCommandBufferRelease(commandBuffer);

		wgpuCommandEncoderRelease(gpuContext.commandEncoder);
		gpuContext.commandEncoder = nullptr;

		return true;
	}

	void ConfigureSurface()
	{
		double canvasWidth, canvasHeight;
		emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);

		gpuContext.surfaceConfig.device = gpuContext.device;
		gpuContext.surfaceConfig.format = wgpuSurfaceGetPreferredFormat(gpuContext.surface, gpuContext.adapter);
		gpuContext.surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
		gpuContext.surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
		gpuContext.surfaceConfig.width = (uint32_t)canvasWidth;
		gpuContext.surfaceConfig.height = (uint32_t)canvasHeight;
		gpuContext.surfaceConfig.presentMode = WGPUPresentMode_Fifo;

		wgpuSurfaceConfigure(gpuContext.surface, &gpuContext.surfaceConfig);

		printf("Configured surface: (%d x %d)\n", gpuContext.surfaceConfig.width, gpuContext.surfaceConfig.height);
	}
} // namespace Bk

int main(int argc, char** argv)
{
	using namespace Bk;

	appContext.arena.Initialize(BK_MEGABYTES(8));
	appContext.desc = Main(argc, argv);

	gpuContext.pipelines.Initialize(&appContext.arena, 32);
	gpuContext.buffers.Initialize(&appContext.arena, 32);
	gpuContext.bindGroupLayouts.Initialize(&appContext.arena, 32);

	gpuContext.instance = wgpuCreateInstance(nullptr);
	wgpuInstanceRequestAdapter(gpuContext.instance, nullptr, Bk::OnAdapterAcquired, nullptr);

	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, Bk::OnResize);
	emscripten_request_animation_frame_loop(Bk::OnFrame, nullptr);

	return 0;
}
