#include "BkGpu.h"

#include "BkPool.h"

#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

namespace Bk
{
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

	struct GpuBindingLayout
	{
		WGPUBindGroupLayout handle;
	};

	struct GpuBindingGroup
	{
		WGPUBindGroup handle;
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

		Arena arena;

		TPool<GpuPipeline> pipelines;
		TPool<GpuBuffer> buffers;
		TPool<GpuBindingLayout> bindingLayouts;
		TPool<GpuBindingGroup> bindingGroups;
	} gpuContext;

	static WGPUStringView WgpuConvert(String string)
	{
		return (WGPUStringView){ .data = string.data, .length = string.length };
	}

	void OnDeviceError(const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
	{
		printf("Device Error (%0x): %.*s\n", type, (int32)message.length, message.data);
	}

	void OnDeviceLost(const WGPUDevice* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2)
	{
		printf("Device Lost (%0x): %.*s\n", reason, (int32)message.length, message.data);
	}

	void ConfigureSurface()
	{
		if (!gpuContext.surface)
		{
			return;
		}

		double canvasWidth, canvasHeight;
		emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);

		gpuContext.surfaceConfig.device = gpuContext.device;
		gpuContext.surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
		gpuContext.surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
		gpuContext.surfaceConfig.width = (uint32_t)canvasWidth;
		gpuContext.surfaceConfig.height = (uint32_t)canvasHeight;
		gpuContext.surfaceConfig.presentMode = WGPUPresentMode_Fifo;

		wgpuSurfaceConfigure(gpuContext.surface, &gpuContext.surfaceConfig);

		printf("Configured surface: (%d x %d)\n", gpuContext.surfaceConfig.width, gpuContext.surfaceConfig.height);
	}

	bool OnResize(int eventType, const EmscriptenUiEvent* event, void* userData)
	{
		ConfigureSurface();

		return true;
	}

	void OnDeviceAcquired(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
	{
		if (status != WGPURequestDeviceStatus_Success)
		{
			FatalError(1, "Failed to acquire WebGPU device: %.*s", (int32)message.length, message.data);
		}

		gpuContext.device = device;
		BK_ASSERTF(gpuContext.device, "Failed to acquire WebGPU device");

		gpuContext.queue = wgpuDeviceGetQueue(gpuContext.device);
		BK_ASSERTF(gpuContext.queue, "Failed to acquire WebGPU device queue");

		WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
		canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
		canvasDesc.selector = WgpuConvert("canvas");

		WGPUSurfaceDescriptor surfaceDesc = {};
		surfaceDesc.nextInChain = &canvasDesc.chain;

		gpuContext.surface = wgpuInstanceCreateSurface(gpuContext.instance, &surfaceDesc);
		BK_ASSERTF(gpuContext.surface, "Failed to create WebGPU surface");

		WGPUSurfaceCapabilities surfaceCapabilities = {};
		wgpuSurfaceGetCapabilities(gpuContext.surface, gpuContext.adapter, &surfaceCapabilities);

		gpuContext.surfaceConfig.format = surfaceCapabilities.formatCount > 0 ? surfaceCapabilities.formats[0] : WGPUTextureFormat_Undefined;

		ConfigureSurface();
	}

	void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
	{
		if (status != WGPURequestAdapterStatus_Success)
		{
			FatalError(1, "Failed to acquire WebGPU adapter: %.*s", (int32)message.length, message.data);
		}

		gpuContext.adapter = adapter;
		BK_ASSERTF(gpuContext.adapter, "Failed to acquire WebGPU adapter");

		WGPUDeviceDescriptor deviceDesc = {};
		deviceDesc.uncapturedErrorCallbackInfo.callback = OnDeviceError;
		deviceDesc.deviceLostCallbackInfo.callback = OnDeviceLost;

		wgpuAdapterRequestDevice(gpuContext.adapter, &deviceDesc, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnDeviceAcquired });
	}

	void GpuInitialize()
	{
		gpuContext.pipelines.Initialize(&gpuContext.arena, 32);
		gpuContext.buffers.Initialize(&gpuContext.arena, 32);
		gpuContext.bindingLayouts.Initialize(&gpuContext.arena, 32);
		gpuContext.bindingGroups.Initialize(&gpuContext.arena, 32);

		gpuContext.instance = wgpuCreateInstance(nullptr);
		BK_ASSERTF(gpuContext.instance, "Failed to create WebGPU instance");

		wgpuInstanceRequestAdapter(gpuContext.instance, nullptr, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnAdapterAcquired });

		emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, OnResize);
	}

	static WGPUVertexFormat WgpuConvert(const GpuVertexFormat value)
	{
		switch (value)
		{
			case GpuVertexFormat::Float32: return WGPUVertexFormat_Float32;
			case GpuVertexFormat::Float32x2: return WGPUVertexFormat_Float32x2;
			case GpuVertexFormat::Float32x3: return WGPUVertexFormat_Float32x3;
			case GpuVertexFormat::Float32x4: return WGPUVertexFormat_Float32x4;
		}
	}

	static WGPUIndexFormat WgpuConvert(const GpuIndexFormat value)
	{
		switch (value)
		{
			case GpuIndexFormat::Uint32: return WGPUIndexFormat_Uint32;
			case GpuIndexFormat::Uint16: return WGPUIndexFormat_Uint16;
		}
	};

	static WGPUShaderStage WgpuConvert(const GpuBindingStage value)
	{
		WGPUShaderStage result = WGPUShaderStage_None;

		if (EnumHasAnyFlags(value, GpuBindingStage::Vertex))
		{
			result |= WGPUShaderStage_Vertex;
		}

		if (EnumHasAnyFlags(value, GpuBindingStage::Pixel))
		{
			result |= WGPUShaderStage_Fragment;
		}

		if (EnumHasAnyFlags(value, GpuBindingStage::Compute))
		{
			result |= WGPUShaderStage_Compute;
		}

		return result;
	}

	static WGPUBufferBindingType WgpuConvert(const GpuBindingType value)
	{
		switch (value)
		{
			case GpuBindingType::None:
				return WGPUBufferBindingType_Undefined;

			case GpuBindingType::UniformBuffer:
			case GpuBindingType::DynamicUniformBuffer:
				return WGPUBufferBindingType_Uniform;

			case GpuBindingType::StorageBuffer:
			case GpuBindingType::DynamicStorageBuffer:
				return WGPUBufferBindingType_Storage;
		}
	}

	uint32 CreatePipeline(const GpuPipelineDesc& desc)
	{
		WGPURenderPipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = WgpuConvert(desc.name);
		pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		pipelineDesc.primitive.frontFace = WGPUFrontFace_CW;
		pipelineDesc.primitive.cullMode = WGPUCullMode_Back;
		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = 0xFFFFFFFF;

		if (desc.VS.code)
		{
			WGPUShaderSourceWGSL shaderSourceDesc = {};
			shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
			shaderSourceDesc.code = WgpuConvert(desc.VS.code);

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderSourceDesc.chain;

			pipelineDesc.vertex.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			pipelineDesc.vertex.entryPoint = WgpuConvert(desc.VS.entryPoint);

			// #TODO: Temp arena allocations
			WGPUVertexBufferLayout vertexBuffers[8] = {};
			WGPUVertexAttribute vertexAttributes[8][16] = {};

			pipelineDesc.vertex.buffers = vertexBuffers;
			pipelineDesc.vertex.bufferCount = desc.VS.buffers.length;

			for (size_t bufferIdx = 0; bufferIdx < desc.VS.buffers.length; ++bufferIdx)
			{
				const GpuVertexBufferDesc& bufferDesc = desc.VS.buffers[bufferIdx];

				vertexBuffers[bufferIdx].arrayStride = bufferDesc.stride;
				vertexBuffers[bufferIdx].attributes = vertexAttributes[bufferIdx];
				vertexBuffers[bufferIdx].attributeCount = bufferDesc.attributes.length;

				for (size_t attributeIdx = 0; attributeIdx < bufferDesc.attributes.length; ++attributeIdx)
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
			WGPUShaderSourceWGSL shaderSourceDesc = {};
			shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
			shaderSourceDesc.code = WgpuConvert(desc.PS.code);

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderSourceDesc.chain;

			WGPUFragmentState fragmentState = {};
			fragmentState.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			fragmentState.entryPoint = WgpuConvert(desc.PS.entryPoint);

			WGPUColorTargetState surfaceTarget = { .format = gpuContext.surfaceConfig.format, .writeMask = WGPUColorWriteMask_All };
			fragmentState.targets = &surfaceTarget;
			fragmentState.targetCount = 1;

			pipelineDesc.fragment = &fragmentState;
		}

		if (desc.bindingLayouts.length > 0)
		{
			WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
			pipelineLayoutDesc.label = WgpuConvert(desc.name);

			// #TODO: Temp arena allocations
			WGPUBindGroupLayout bindingLayouts[8];

			pipelineLayoutDesc.bindGroupLayouts = bindingLayouts;
			pipelineLayoutDesc.bindGroupLayoutCount = desc.bindingLayouts.length;

			for (size_t layoutIdx = 0; layoutIdx < desc.bindingLayouts.length; ++layoutIdx)
			{
				bindingLayouts[layoutIdx] = gpuContext.bindingLayouts.GetItem(desc.bindingLayouts[layoutIdx])->handle;
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

	void DestroyPipeline(uint32 handle)
	{
		GpuPipeline* pipeline = gpuContext.pipelines.GetItem(handle);
		if (pipeline)
		{
			wgpuRenderPipelineRelease(pipeline->handle);
			gpuContext.pipelines.FreeItem(handle);
		}
	}

	uint32 CreateBuffer(const GpuBufferDesc& desc)
	{
		// Allow shorthand of not specifying buffer size if passing data
		const uint64 bufferSize = desc.size == 0 ? desc.data.length : desc.size;

		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.label = WgpuConvert(desc.name);
		bufferDesc.size = AlignUp(bufferSize, 4); // Mapping requires size to be multiple of 4
		bufferDesc.mappedAtCreation = desc.data.length != 0;

		switch (desc.type)
		{
			case GpuBufferType::Uniform: bufferDesc.usage = WGPUBufferUsage_Uniform; break;
			case GpuBufferType::Storage: bufferDesc.usage = WGPUBufferUsage_Storage; break;
			case GpuBufferType::Vertex: bufferDesc.usage = WGPUBufferUsage_Vertex; break;
			case GpuBufferType::Index: bufferDesc.usage = WGPUBufferUsage_Index; break;
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
			void* bufferMappedPtr = wgpuBufferGetMappedRange(buffer, 0, AlignUp(bufferMappedSize, 4));

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

	void WriteBuffer(uint32 handle, TSpan<uint8> data, uint64 offset)
	{
		GpuBuffer* buffer = gpuContext.buffers.GetItem(handle);
		if (buffer)
		{
			wgpuQueueWriteBuffer(gpuContext.queue, buffer->handle, offset, data.data, data.length);
		}
	}

	void DestroyBuffer(uint32 handle)
	{
		GpuBuffer* buffer = gpuContext.buffers.GetItem(handle);
		if (buffer)
		{
			wgpuBufferRelease(buffer->handle);
			gpuContext.buffers.FreeItem(handle);
		}
	}

	uint32 CreateBindingLayout(const GpuBindingLayoutDesc& desc)
	{
		WGPUBindGroupLayoutDescriptor bindingLayoutDesc = {};
		bindingLayoutDesc.label = WgpuConvert(desc.name);

		// #TODO: Temp arena allocations
		WGPUBindGroupLayoutEntry bindings[8] = {};

		bindingLayoutDesc.entries = bindings;
		bindingLayoutDesc.entryCount = desc.bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < desc.bindings.length; ++bindingIdx)
		{
			const GpuBindingLayoutEntry& binding = desc.bindings[bindingIdx];

			bindings[bindingIdx].binding = bindingIdx;
			bindings[bindingIdx].visibility = WgpuConvert(binding.stage);

			bindings[bindingIdx].buffer.type = WgpuConvert(binding.type);
			if (binding.type == GpuBindingType::DynamicStorageBuffer || binding.type == GpuBindingType::DynamicUniformBuffer)
			{
				bindings[bindingIdx].buffer.hasDynamicOffset = true;
			}
		}

		WGPUBindGroupLayout bindingLayout = wgpuDeviceCreateBindGroupLayout(gpuContext.device, &bindingLayoutDesc);

		uint32 bindingLayoutHandle = 0;
		if (bindingLayout)
		{
			GpuBindingLayout* bindingLayoutWrapper = gpuContext.bindingLayouts.AllocateItem(&bindingLayoutHandle);
			bindingLayoutWrapper->handle = bindingLayout;
		}

		return bindingLayoutHandle;
	}

	void DestroyBindingLayout(uint32 handle)
	{
		GpuBindingLayout* bindingLayout = gpuContext.bindingLayouts.GetItem(handle);
		if (bindingLayout)
		{
			wgpuBindGroupLayoutRelease(bindingLayout->handle);
			gpuContext.bindingLayouts.FreeItem(handle);
		}
	}

	uint32 CreateBindingGroup(const GpuBindingGroupDesc& desc)
	{
		WGPUBindGroupDescriptor bindingGroupDesc = {};
		bindingGroupDesc.label = WgpuConvert(desc.name);
		bindingGroupDesc.layout = gpuContext.bindingLayouts.GetItem(desc.bindingLayout)->handle;

		// #TODO: Temp arena allocations
		WGPUBindGroupEntry bindings[8] = {};

		bindingGroupDesc.entries = bindings;
		bindingGroupDesc.entryCount = desc.bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < desc.bindings.length; ++bindingIdx)
		{
			const GpuBindingGroupEntry& binding = desc.bindings[bindingIdx];

			bindings[bindingIdx].binding = bindingIdx;
			if (const GpuBuffer* buffer = gpuContext.buffers.GetItem(binding.buffer))
			{
				BK_ASSERT(binding.bufferOffset < buffer->size);
				bindings[bindingIdx].buffer = buffer->handle;
				bindings[bindingIdx].offset = binding.bufferOffset;
				bindings[bindingIdx].size = buffer->size - binding.bufferOffset;
			}
		}

		WGPUBindGroup bindingGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &bindingGroupDesc);

		uint32 bindingGroupHandle = 0;
		if (bindingGroup)
		{
			GpuBindingGroup* bindingGroupWrapper = gpuContext.bindingGroups.AllocateItem(&bindingGroupHandle);
			bindingGroupWrapper->handle = bindingGroup;
		}

		return bindingGroupHandle;
	}

	void DestroyBindingGroup(uint32 handle)
	{
		GpuBindingGroup* bindingGroup = gpuContext.bindingGroups.GetItem(handle);
		if (bindingGroup)
		{
			wgpuBindGroupRelease(bindingGroup->handle);
			gpuContext.bindingGroups.FreeItem(handle);
		}
	}

	bool BeginFrame()
	{
		if (!gpuContext.device)
		{
			return false;
		}

		gpuContext.commandEncoder = wgpuDeviceCreateCommandEncoder(gpuContext.device, nullptr);

		return true;
	}

	bool EndFrame()
	{
		if (!gpuContext.commandEncoder)
		{
			return false;
		}

		WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(gpuContext.commandEncoder, nullptr);
		wgpuQueueSubmit(gpuContext.queue, 1, &commandBuffer);
		wgpuCommandBufferRelease(commandBuffer);

		wgpuCommandEncoderRelease(gpuContext.commandEncoder);
		gpuContext.commandEncoder = nullptr;

		return true;
	}

	void BeginPass(const GpuPassDesc& desc)
	{
		BK_ASSERT(gpuContext.renderPassEncoder == nullptr);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.label = WgpuConvert(desc.name);

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

		for (size_t groupIdx = 0; groupIdx < desc.bindingGroups.length; ++groupIdx)
		{
			GpuBindingGroup* group = gpuContext.bindingGroups.GetItem(desc.bindingGroups[groupIdx]);
			wgpuRenderPassEncoderSetBindGroup(gpuContext.renderPassEncoder, groupIdx, group ? group->handle : nullptr, 0, nullptr);
		}

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
				desc.indexOffset, (int32)desc.vertexOffset, desc.instanceOffset); // #TODO: Why is baseVertex signed?
		}
		else
		{
			wgpuRenderPassEncoderDraw(
				gpuContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.vertexOffset, desc.instanceOffset);
		}
	}
}
