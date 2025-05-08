#include "BkGraphics.h"

#include "BkPool.h"

#include <emscripten/html5.h>
#include <webgpu/webgpu.h>

namespace Bk
{
	struct GfxPipeline
	{
		WGPURenderPipeline handle;
		WGPUIndexFormat indexFormat;
	};

	struct GfxBuffer
	{
		WGPUBuffer handle;
		uint64 size;
	};

	struct GfxBindingLayout
	{
		WGPUBindGroupLayout handle;
	};

	struct GfxBindingGroup
	{
		WGPUBindGroup handle;
	};

	struct GfxContext
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

		TPool<GfxPipeline> pipelines;
		TPool<GfxBuffer> buffers;
		TPool<GfxBindingLayout> bindingLayouts;
		TPool<GfxBindingGroup> bindingGroups;
	} gfxContext;

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
		if (!gfxContext.surface)
		{
			return;
		}

		double canvasWidth, canvasHeight;
		emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);

		gfxContext.surfaceConfig.device = gfxContext.device;
		gfxContext.surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
		gfxContext.surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
		gfxContext.surfaceConfig.width = (uint32_t)canvasWidth;
		gfxContext.surfaceConfig.height = (uint32_t)canvasHeight;
		gfxContext.surfaceConfig.presentMode = WGPUPresentMode_Fifo;

		wgpuSurfaceConfigure(gfxContext.surface, &gfxContext.surfaceConfig);

		printf("Configured surface: (%d x %d)\n", gfxContext.surfaceConfig.width, gfxContext.surfaceConfig.height);
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

		gfxContext.device = device;
		BK_ASSERTF(gfxContext.device, "Failed to acquire WebGPU device");

		gfxContext.queue = wgpuDeviceGetQueue(gfxContext.device);
		BK_ASSERTF(gfxContext.queue, "Failed to acquire WebGPU device queue");

		WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
		canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
		canvasDesc.selector = WgpuConvert("canvas");

		WGPUSurfaceDescriptor surfaceDesc = {};
		surfaceDesc.nextInChain = &canvasDesc.chain;

		gfxContext.surface = wgpuInstanceCreateSurface(gfxContext.instance, &surfaceDesc);
		BK_ASSERTF(gfxContext.surface, "Failed to create WebGPU surface");

		WGPUSurfaceCapabilities surfaceCapabilities = {};
		wgpuSurfaceGetCapabilities(gfxContext.surface, gfxContext.adapter, &surfaceCapabilities);

		gfxContext.surfaceConfig.format = surfaceCapabilities.formatCount > 0 ? surfaceCapabilities.formats[0] : WGPUTextureFormat_Undefined;

		ConfigureSurface();
	}

	void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
	{
		if (status != WGPURequestAdapterStatus_Success)
		{
			FatalError(1, "Failed to acquire WebGPU adapter: %.*s", (int32)message.length, message.data);
		}

		gfxContext.adapter = adapter;
		BK_ASSERTF(gfxContext.adapter, "Failed to acquire WebGPU adapter");

		WGPUDeviceDescriptor deviceDesc = {};
		deviceDesc.uncapturedErrorCallbackInfo.callback = OnDeviceError;
		deviceDesc.deviceLostCallbackInfo.callback = OnDeviceLost;

		wgpuAdapterRequestDevice(gfxContext.adapter, &deviceDesc, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnDeviceAcquired });
	}

	void GfxInitialize()
	{
		gfxContext.pipelines.Initialize(&gfxContext.arena, 32);
		gfxContext.buffers.Initialize(&gfxContext.arena, 32);
		gfxContext.bindingLayouts.Initialize(&gfxContext.arena, 32);
		gfxContext.bindingGroups.Initialize(&gfxContext.arena, 32);

		gfxContext.instance = wgpuCreateInstance(nullptr);
		BK_ASSERTF(gfxContext.instance, "Failed to create WebGPU instance");

		wgpuInstanceRequestAdapter(gfxContext.instance, nullptr, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnAdapterAcquired });

		emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, OnResize);
	}

	static WGPUVertexFormat WgpuConvert(const GfxVertexFormat value)
	{
		switch (value)
		{
			case GfxVertexFormat::Float32: return WGPUVertexFormat_Float32;
			case GfxVertexFormat::Float32x2: return WGPUVertexFormat_Float32x2;
			case GfxVertexFormat::Float32x3: return WGPUVertexFormat_Float32x3;
			case GfxVertexFormat::Float32x4: return WGPUVertexFormat_Float32x4;
		}
	}

	static WGPUIndexFormat WgpuConvert(const GfxIndexFormat value)
	{
		switch (value)
		{
			case GfxIndexFormat::Uint32: return WGPUIndexFormat_Uint32;
			case GfxIndexFormat::Uint16: return WGPUIndexFormat_Uint16;
		}
	};

	static WGPUShaderStage WgpuConvert(const GfxBindingStage value)
	{
		WGPUShaderStage result = WGPUShaderStage_None;

		if (EnumHasAnyFlags(value, GfxBindingStage::Vertex))
		{
			result |= WGPUShaderStage_Vertex;
		}

		if (EnumHasAnyFlags(value, GfxBindingStage::Pixel))
		{
			result |= WGPUShaderStage_Fragment;
		}

		if (EnumHasAnyFlags(value, GfxBindingStage::Compute))
		{
			result |= WGPUShaderStage_Compute;
		}

		return result;
	}

	static WGPUBufferBindingType WgpuConvert(const GfxBindingType value)
	{
		switch (value)
		{
			case GfxBindingType::None:
				return WGPUBufferBindingType_Undefined;

			case GfxBindingType::UniformBuffer:
			case GfxBindingType::DynamicUniformBuffer:
				return WGPUBufferBindingType_Uniform;

			case GfxBindingType::StorageBuffer:
			case GfxBindingType::DynamicStorageBuffer:
				return WGPUBufferBindingType_Storage;
		}
	}

	uint32 CreatePipeline(const GfxPipelineDesc& desc)
	{
		WGPURenderPipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = WgpuConvert(desc.name);
		pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = 0xFFFFFFFF;

		if (desc.VS.code)
		{
			WGPUShaderSourceWGSL shaderSourceDesc = {};
			shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
			shaderSourceDesc.code = WgpuConvert(desc.VS.code);

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderSourceDesc.chain;

			pipelineDesc.vertex.module = wgpuDeviceCreateShaderModule(gfxContext.device, &shaderDesc);
			pipelineDesc.vertex.entryPoint = WgpuConvert(desc.VS.entryPoint);

			// #TODO: Temp arena allocations
			WGPUVertexBufferLayout vertexBuffers[8] = {};
			WGPUVertexAttribute vertexAttributes[8][16] = {};

			pipelineDesc.vertex.buffers = vertexBuffers;
			pipelineDesc.vertex.bufferCount = desc.VS.buffers.length;

			for (size_t bufferIdx = 0; bufferIdx < desc.VS.buffers.length; ++bufferIdx)
			{
				const GfxVertexBufferDesc& bufferDesc = desc.VS.buffers[bufferIdx];

				vertexBuffers[bufferIdx].arrayStride = bufferDesc.stride;
				vertexBuffers[bufferIdx].attributes = vertexAttributes[bufferIdx];
				vertexBuffers[bufferIdx].attributeCount = bufferDesc.attributes.length;

				for (size_t attributeIdx = 0; attributeIdx < bufferDesc.attributes.length; ++attributeIdx)
				{
					const GfxVertexBufferAttribute& attributeDesc = bufferDesc.attributes[attributeIdx];

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
			fragmentState.module = wgpuDeviceCreateShaderModule(gfxContext.device, &shaderDesc);
			fragmentState.entryPoint = WgpuConvert(desc.PS.entryPoint);

			WGPUColorTargetState surfaceTarget = { .format = gfxContext.surfaceConfig.format, .writeMask = WGPUColorWriteMask_All };
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
				bindingLayouts[layoutIdx] = gfxContext.bindingLayouts.GetItem(desc.bindingLayouts[layoutIdx])->handle;
			}

			pipelineDesc.layout = wgpuDeviceCreatePipelineLayout(gfxContext.device, &pipelineLayoutDesc);
		}

		WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(gfxContext.device, &pipelineDesc);

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
			GfxPipeline* pipelineWrapper = gfxContext.pipelines.AllocateItem(&pipelineHandle);
			pipelineWrapper->handle = pipeline;
			pipelineWrapper->indexFormat = WgpuConvert(desc.indexFormat);
		}

		return pipelineHandle;
	}

	void DestroyPipeline(uint32 handle)
	{
		GfxPipeline* pipeline = gfxContext.pipelines.GetItem(handle);
		if (pipeline)
		{
			if (pipeline->handle)
			{
				wgpuRenderPipelineRelease(pipeline->handle);
			}

			gfxContext.pipelines.FreeItem(handle);
		}
	}

	uint32 CreateBuffer(const GfxBufferDesc& desc)
	{
		// Allow shorthand of not specifying buffer size if passing data
		const uint64 bufferSize = desc.size == 0 ? desc.data.length : desc.size;

		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.label = WgpuConvert(desc.name);
		bufferDesc.size = AlignUp(bufferSize, 4); // Mapping requires size to be multiple of 4
		bufferDesc.mappedAtCreation = desc.data.length != 0;

		switch (desc.type)
		{
			case GfxBufferType::Uniform: bufferDesc.usage = WGPUBufferUsage_Uniform; break;
			case GfxBufferType::Storage: bufferDesc.usage = WGPUBufferUsage_Storage; break;
			case GfxBufferType::Vertex: bufferDesc.usage = WGPUBufferUsage_Vertex; break;
			case GfxBufferType::Index: bufferDesc.usage = WGPUBufferUsage_Index; break;
		}

		switch (desc.access)
		{
			case GfxBufferAccess::GpuOnly: bufferDesc.usage |= WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst; break;
			case GfxBufferAccess::CpuRead: bufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst; break;
			case GfxBufferAccess::CpuWrite: bufferDesc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc; break;
		}

		WGPUBuffer buffer = wgpuDeviceCreateBuffer(gfxContext.device, &bufferDesc);
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
			GfxBuffer* bufferWrapper = gfxContext.buffers.AllocateItem(&bufferHandle);
			bufferWrapper->handle = buffer;
			bufferWrapper->size = bufferSize;
		}

		return bufferHandle;
	}

	void DestroyBuffer(uint32 handle)
	{
		GfxBuffer* buffer = gfxContext.buffers.GetItem(handle);
		if (buffer)
		{
			if (buffer->handle)
			{
				wgpuBufferRelease(buffer->handle);
			}

			gfxContext.buffers.FreeItem(handle);
		}
	}

	uint32 CreateBindingLayout(const GfxBindingLayoutDesc& desc)
	{
		WGPUBindGroupLayoutDescriptor bindingLayoutDesc = {};
		bindingLayoutDesc.label = WgpuConvert(desc.name);

		// #TODO: Temp arena allocations
		WGPUBindGroupLayoutEntry bindings[8] = {};

		bindingLayoutDesc.entries = bindings;
		bindingLayoutDesc.entryCount = desc.bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < desc.bindings.length; ++bindingIdx)
		{
			const GfxBindingLayoutEntry& binding = desc.bindings[bindingIdx];

			bindings[bindingIdx].binding = bindingIdx;
			bindings[bindingIdx].visibility = WgpuConvert(binding.stage);

			bindings[bindingIdx].buffer.type = WgpuConvert(binding.type);
			if (binding.type == GfxBindingType::DynamicStorageBuffer || binding.type == GfxBindingType::DynamicUniformBuffer)
			{
				bindings[bindingIdx].buffer.hasDynamicOffset = true;
			}
		}

		WGPUBindGroupLayout bindingLayout = wgpuDeviceCreateBindGroupLayout(gfxContext.device, &bindingLayoutDesc);

		uint32 bindingLayoutHandle = 0;
		if (bindingLayout)
		{
			GfxBindingLayout* bindingLayoutWrapper = gfxContext.bindingLayouts.AllocateItem(&bindingLayoutHandle);
			bindingLayoutWrapper->handle = bindingLayout;
		}

		return bindingLayoutHandle;
	}

	void DestroyBindingLayout(uint32 handle)
	{
		GfxBindingLayout* bindingLayout = gfxContext.bindingLayouts.GetItem(handle);
		if (bindingLayout)
		{
			if (bindingLayout->handle)
			{
				wgpuBindGroupLayoutRelease(bindingLayout->handle);
			}

			gfxContext.bindingLayouts.FreeItem(handle);
		}
	}

	uint32 CreateBindingGroup(const GfxBindingGroupDesc& desc)
	{
		WGPUBindGroupDescriptor bindingGroupDesc = {};
		bindingGroupDesc.label = WgpuConvert(desc.name);
		bindingGroupDesc.layout = gfxContext.bindingLayouts.GetItem(desc.bindingLayout)->handle;

		// #TODO: Temp arena allocations
		WGPUBindGroupEntry bindings[8] = {};

		bindingGroupDesc.entries = bindings;
		bindingGroupDesc.entryCount = desc.bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < desc.bindings.length; ++bindingIdx)
		{
			const GfxBindingGroupEntry& binding = desc.bindings[bindingIdx];

			bindings[bindingIdx].binding = bindingIdx;
			if (const GfxBuffer* buffer = gfxContext.buffers.GetItem(binding.buffer))
			{
				BK_ASSERT(binding.bufferOffset < buffer->size);
				bindings[bindingIdx].buffer = buffer->handle;
				bindings[bindingIdx].offset = binding.bufferOffset;
				bindings[bindingIdx].size = buffer->size - binding.bufferOffset;
			}
		}

		WGPUBindGroup bindingGroup = wgpuDeviceCreateBindGroup(gfxContext.device, &bindingGroupDesc);

		uint32 bindingGroupHandle = 0;
		if (bindingGroup)
		{
			GfxBindingGroup* bindingGroupWrapper = gfxContext.bindingGroups.AllocateItem(&bindingGroupHandle);
			bindingGroupWrapper->handle = bindingGroup;
		}

		return bindingGroupHandle;
	}

	void DestroyBindingGroup(uint32 handle)
	{
		GfxBindingGroup* bindingGroup = gfxContext.bindingGroups.GetItem(handle);
		if (bindingGroup)
		{
			if (bindingGroup->handle)
			{
				wgpuBindGroupRelease(bindingGroup->handle);
			}

			gfxContext.bindingGroups.FreeItem(handle);
		}
	}

	bool BeginFrame()
	{
		if (!gfxContext.device)
		{
			return false;
		}

		gfxContext.commandEncoder = wgpuDeviceCreateCommandEncoder(gfxContext.device, nullptr);

		return true;
	}

	bool EndFrame()
	{
		if (!gfxContext.commandEncoder)
		{
			return false;
		}

		WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(gfxContext.commandEncoder, nullptr);
		wgpuQueueSubmit(gfxContext.queue, 1, &commandBuffer);
		wgpuCommandBufferRelease(commandBuffer);

		wgpuCommandEncoderRelease(gfxContext.commandEncoder);
		gfxContext.commandEncoder = nullptr;

		return true;
	}

	void BeginPass(const GfxPassDesc& desc)
	{
		BK_ASSERT(gfxContext.renderPassEncoder == nullptr);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.label = WgpuConvert(desc.name);

		WGPUSurfaceTexture surfaceTexture;
		wgpuSurfaceGetCurrentTexture(gfxContext.surface, &surfaceTexture);

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

		gfxContext.renderPassEncoder = wgpuCommandEncoderBeginRenderPass(gfxContext.commandEncoder, &passDesc);
	}

	void EndPass()
	{
		BK_ASSERT(gfxContext.renderPassEncoder != nullptr);

		wgpuRenderPassEncoderEnd(gfxContext.renderPassEncoder);
		wgpuRenderPassEncoderRelease(gfxContext.renderPassEncoder);
		gfxContext.renderPassEncoder = nullptr;
	}

	void Draw(const GfxDrawDesc& desc)
	{
		BK_ASSERT(gfxContext.renderPassEncoder != nullptr);
		BK_ASSERT(desc.pipeline);

		GfxPipeline* pipeline = gfxContext.pipelines.GetItem(desc.pipeline);
		wgpuRenderPassEncoderSetPipeline(gfxContext.renderPassEncoder, pipeline->handle);

		for (size_t groupIdx = 0; groupIdx < desc.bindingGroups.length; ++groupIdx)
		{
			GfxBindingGroup* group = gfxContext.bindingGroups.GetItem(desc.bindingGroups[groupIdx]);
			wgpuRenderPassEncoderSetBindGroup(gfxContext.renderPassEncoder, groupIdx, group ? group->handle : nullptr, 0, nullptr);
		}

		if (desc.vertexBuffer)
		{
			GfxBuffer* buffer = gfxContext.buffers.GetItem(desc.vertexBuffer);
			wgpuRenderPassEncoderSetVertexBuffer(gfxContext.renderPassEncoder, 0, buffer->handle, 0, buffer->size);
		}

		if (desc.indexBuffer)
		{
			GfxBuffer* buffer = gfxContext.buffers.GetItem(desc.indexBuffer);
			wgpuRenderPassEncoderSetIndexBuffer(gfxContext.renderPassEncoder, buffer->handle, pipeline->indexFormat, 0, buffer->size);

			wgpuRenderPassEncoderDrawIndexed(
				gfxContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.indexOffset, (int32)desc.vertexOffset, desc.instanceOffset); // #TODO: Why is baseVertex signed?
		}
		else
		{
			wgpuRenderPassEncoderDraw(
				gfxContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.vertexOffset, desc.instanceOffset);
		}
	}
}
