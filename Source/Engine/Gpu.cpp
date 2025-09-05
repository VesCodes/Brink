#include "Gpu.h"

#include "Core/Pool.h"

#if BK_PLATFORM_EMSCRIPTEN
#include <webgpu/webgpu.h>
#else
#include <dawn/webgpu.h>
#endif

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

	struct GpuSurface
	{
		WGPUSurface handle;
		WGPUSurfaceConfiguration config;

		WGPUTexture depthTexture;
		WGPUTextureView depthTextureView;
	};

	struct GpuContext
	{
		Arena arena;

		TPool<GpuPipeline> pipelines;
		TPool<GpuBuffer> buffers;
		TPool<GpuBindingLayout> bindingLayouts;
		TPool<GpuBindingGroup> bindingGroups;
		TPool<GpuSurface> surfaces;

		WGPUInstance instance;
		WGPUAdapter adapter;
		WGPUDevice device;
		WGPUQueue queue;

		WGPUCommandEncoder commandEncoder;
		WGPURenderPassEncoder renderPassEncoder;
	} gpuContext;

	static WGPUStringView WgpuConvert(String string)
	{
		return WGPUStringView{ .data = string.data, .length = string.length != 0 ? string.length : WGPU_STRLEN };
	}

	void OnDeviceError(const WGPUDevice* device, WGPUErrorType type, WGPUStringView message, void* userdata1, void* userdata2)
	{
		printf("Device Error (%0x): %.*s\n", type, static_cast<int32>(message.length), message.data);
	}

	void OnDeviceLost(const WGPUDevice* device, WGPUDeviceLostReason reason, WGPUStringView message, void* userdata1, void* userdata2)
	{
		printf("Device Lost (%0x): %.*s\n", reason, static_cast<int32>(message.length), message.data);
	}

	void OnDeviceAcquired(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata1, void* userdata2)
	{
		if (status != WGPURequestDeviceStatus_Success)
		{
			FatalError(1, "Failed to acquire WebGPU device: %.*s", static_cast<int32>(message.length), message.data);
		}

		gpuContext.device = device;
		BK_ASSERTF(gpuContext.device, "Failed to acquire WebGPU device");

		gpuContext.queue = wgpuDeviceGetQueue(gpuContext.device);
		BK_ASSERTF(gpuContext.queue, "Failed to acquire WebGPU device queue");
	}

	void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
	{
		if (status != WGPURequestAdapterStatus_Success)
		{
			FatalError(1, "Failed to acquire WebGPU adapter: %.*s", static_cast<int32>(message.length), message.data);
		}

		gpuContext.adapter = adapter;
		BK_ASSERTF(gpuContext.adapter, "Failed to acquire WebGPU adapter");

		WGPUDeviceDescriptor deviceDesc = {};
		deviceDesc.uncapturedErrorCallbackInfo.callback = OnDeviceError;
		deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
		deviceDesc.deviceLostCallbackInfo.callback = OnDeviceLost;

#if !BK_PLATFORM_EMSCRIPTEN
		const char* enabledToggles[] = {
			"use_user_defined_labels_in_backend",
		};

		WGPUDawnTogglesDescriptor dawnTogglesDesc = {};
		dawnTogglesDesc.chain.sType = WGPUSType_DawnTogglesDescriptor;
		dawnTogglesDesc.enabledToggles = enabledToggles;
		dawnTogglesDesc.enabledToggleCount = BK_ARRAY_COUNT(enabledToggles);

		deviceDesc.nextInChain = &dawnTogglesDesc.chain;
#endif

		wgpuAdapterRequestDevice(gpuContext.adapter, &deviceDesc, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnDeviceAcquired });
	}

	void GpuInitialize()
	{
		Allocate(gpuContext.arena, gpuContext.pipelines, 512);
		Allocate(gpuContext.arena, gpuContext.buffers, 512);
		Allocate(gpuContext.arena, gpuContext.bindingLayouts, 512);
		Allocate(gpuContext.arena, gpuContext.bindingGroups, 512);
		Allocate(gpuContext.arena, gpuContext.surfaces, 32);

		gpuContext.instance = wgpuCreateInstance(nullptr);
		BK_ASSERTF(gpuContext.instance, "Failed to create WebGPU instance");

		wgpuInstanceRequestAdapter(gpuContext.instance, nullptr, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnAdapterAcquired });
	}

	static WGPUVertexFormat WgpuConvert(const GpuVertexFormat value)
	{
		switch (value)
		{
			case GpuVertexFormat::Uint8: return WGPUVertexFormat_Uint8;
			case GpuVertexFormat::Uint8x2: return WGPUVertexFormat_Uint8x2;
			case GpuVertexFormat::Uint8x4: return WGPUVertexFormat_Uint8x4;
			case GpuVertexFormat::Uint16: return WGPUVertexFormat_Uint16;
			case GpuVertexFormat::Uint16x2: return WGPUVertexFormat_Uint16x2;
			case GpuVertexFormat::Uint16x4: return WGPUVertexFormat_Uint16x4;
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
			case GpuIndexFormat::Uint16: return WGPUIndexFormat_Uint16;
			case GpuIndexFormat::Uint32: return WGPUIndexFormat_Uint32;
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

			case GpuBindingType::ReadOnlyStorageBuffer:
			case GpuBindingType::DynamicReadOnlyStorageBuffer:
				return WGPUBufferBindingType_ReadOnlyStorage;
		}
	}

	uint32 CreatePipeline(const GpuPipelineDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPURenderPipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = WgpuConvert(desc.name);
		pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		pipelineDesc.primitive.frontFace = WGPUFrontFace_CW;
		pipelineDesc.primitive.cullMode = WGPUCullMode_Front;

		WGPUDepthStencilState depthStencil = {};
		depthStencil.format = WGPUTextureFormat_Depth24Plus;
		depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
		depthStencil.depthCompare = WGPUCompareFunction_Less;

		pipelineDesc.depthStencil = &depthStencil;

		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = 0xFFFFFFFF;

		if (desc.vertexShader.code.length != 0)
		{
			WGPUShaderSourceWGSL shaderSourceDesc = {};
			shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
			shaderSourceDesc.code = WgpuConvert(desc.vertexShader.code);

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderSourceDesc.chain;

			pipelineDesc.vertex.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			pipelineDesc.vertex.entryPoint = WgpuConvert(desc.vertexShader.entryPoint);

			TSpan<WGPUVertexBufferLayout> vertexBuffers = PushZeroed<WGPUVertexBufferLayout>(scratch.arena, desc.vertexShader.buffers.length);

			pipelineDesc.vertex.buffers = vertexBuffers;
			pipelineDesc.vertex.bufferCount = vertexBuffers.length;

			uint32 attributeLocation = 0;
			for (size_t bufferIdx = 0; bufferIdx < vertexBuffers.length; ++bufferIdx)
			{
				WGPUVertexBufferLayout& buffer = vertexBuffers[bufferIdx];
				const GpuVertexBufferDesc& bufferDesc = desc.vertexShader.buffers[bufferIdx];

				TSpan<WGPUVertexAttribute> vertexAttributes = PushZeroed<WGPUVertexAttribute>(scratch.arena, bufferDesc.attributes.length);

				buffer.arrayStride = bufferDesc.stride;
				buffer.attributes = vertexAttributes;
				buffer.attributeCount = vertexAttributes.length;

				for (size_t attributeIdx = 0; attributeIdx < vertexAttributes.length; ++attributeIdx)
				{
					WGPUVertexAttribute& attribute = vertexAttributes[attributeIdx];
					const GpuVertexBufferAttribute& attributeDesc = bufferDesc.attributes[attributeIdx];

					attribute.format = WgpuConvert(attributeDesc.format);
					attribute.offset = attributeDesc.offset;
					attribute.shaderLocation = attributeLocation++;
				}
			}
		}

		if (desc.pixelShader.code.length != 0)
		{
			WGPUShaderSourceWGSL shaderSourceDesc = {};
			shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
			shaderSourceDesc.code = WgpuConvert(desc.pixelShader.code);

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderSourceDesc.chain;

			WGPUFragmentState fragmentState = {};
			fragmentState.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			fragmentState.entryPoint = WgpuConvert(desc.pixelShader.entryPoint);

			WGPUColorTargetState surfaceTarget = { .format = WGPUTextureFormat_BGRA8Unorm, .writeMask = WGPUColorWriteMask_All };
			fragmentState.targets = &surfaceTarget;
			fragmentState.targetCount = 1;

			pipelineDesc.fragment = &fragmentState;
		}

		if (desc.bindingLayouts.length != 0)
		{
			WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
			pipelineLayoutDesc.label = WgpuConvert(desc.name);

			TSpan<WGPUBindGroupLayout> bindingLayouts = PushZeroed<WGPUBindGroupLayout>(scratch.arena, desc.bindingLayouts.length);

			pipelineLayoutDesc.bindGroupLayouts = bindingLayouts;
			pipelineLayoutDesc.bindGroupLayoutCount = desc.bindingLayouts.length;

			for (size_t layoutIdx = 0; layoutIdx < bindingLayouts.length; ++layoutIdx)
			{
				bindingLayouts[layoutIdx] = GetSlot(gpuContext.bindingLayouts, desc.bindingLayouts[layoutIdx])->handle;
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
			GpuPipeline* pipelineWrapper = AcquireSlot(gpuContext.pipelines, &pipelineHandle);
			pipelineWrapper->handle = pipeline;
			pipelineWrapper->indexFormat = WgpuConvert(desc.indexFormat);
		}

		return pipelineHandle;
	}

	void DestroyPipeline(uint32 handle)
	{
		GpuPipeline* pipeline = GetSlot(gpuContext.pipelines, handle);
		if (pipeline)
		{
			wgpuRenderPipelineRelease(pipeline->handle);
			ReleaseSlot(gpuContext.pipelines, handle);
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
			const size_t bufferMappedSize = Min(bufferSize, desc.data.length);
			void* bufferMappedPtr = wgpuBufferGetMappedRange(buffer, 0, AlignUp(bufferMappedSize, 4));

			CopyMemory(bufferMappedPtr, desc.data.data, bufferMappedSize);

			wgpuBufferUnmap(buffer);
		}

		uint32 bufferHandle = 0;
		if (buffer)
		{
			GpuBuffer* bufferWrapper = AcquireSlot(gpuContext.buffers, &bufferHandle);
			bufferWrapper->handle = buffer;
			bufferWrapper->size = bufferSize;
		}

		return bufferHandle;
	}

	void WriteBuffer(uint32 handle, TSpan<const uint8> data, uint64 offset)
	{
		GpuBuffer* buffer = GetSlot(gpuContext.buffers, handle);
		if (buffer)
		{
			wgpuQueueWriteBuffer(gpuContext.queue, buffer->handle, offset, data.data, data.length);
		}
	}

	void DestroyBuffer(uint32 handle)
	{
		GpuBuffer* buffer = GetSlot(gpuContext.buffers, handle);
		if (buffer)
		{
			wgpuBufferRelease(buffer->handle);
			ReleaseSlot(gpuContext.buffers, handle);
		}
	}

	uint32 CreateBindingLayout(const GpuBindingLayoutDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPUBindGroupLayoutDescriptor bindingLayoutDesc = {};
		bindingLayoutDesc.label = WgpuConvert(desc.name);

		TSpan<WGPUBindGroupLayoutEntry> bindings = PushZeroed<WGPUBindGroupLayoutEntry>(scratch.arena, desc.bindings.length);

		bindingLayoutDesc.entries = bindings;
		bindingLayoutDesc.entryCount = bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < desc.bindings.length; ++bindingIdx)
		{
			WGPUBindGroupLayoutEntry& binding = bindings[bindingIdx];
			const GpuBindingLayoutEntry& bindingDesc = desc.bindings[bindingIdx];

			binding.binding = bindingIdx;
			binding.visibility = WgpuConvert(bindingDesc.stage);
			binding.bindingArraySize = 1; // #TODO: https://github.com/gpuweb/gpuweb/blob/main/proposals/sized-binding-arrays.md
			binding.buffer.type = WgpuConvert(bindingDesc.type);
			binding.buffer.hasDynamicOffset = (bindingDesc.type == GpuBindingType::DynamicUniformBuffer || bindingDesc.type == GpuBindingType::DynamicStorageBuffer || bindingDesc.type == GpuBindingType::DynamicReadOnlyStorageBuffer);
		}

		WGPUBindGroupLayout bindingLayout = wgpuDeviceCreateBindGroupLayout(gpuContext.device, &bindingLayoutDesc);

		uint32 bindingLayoutHandle = 0;
		if (bindingLayout)
		{
			GpuBindingLayout* bindingLayoutWrapper = AcquireSlot(gpuContext.bindingLayouts, &bindingLayoutHandle);
			bindingLayoutWrapper->handle = bindingLayout;
		}

		return bindingLayoutHandle;
	}

	void DestroyBindingLayout(uint32 handle)
	{
		GpuBindingLayout* bindingLayout = GetSlot(gpuContext.bindingLayouts, handle);
		if (bindingLayout)
		{
			wgpuBindGroupLayoutRelease(bindingLayout->handle);
			ReleaseSlot(gpuContext.bindingLayouts, handle);
		}
	}

	uint32 CreateBindingGroup(const GpuBindingGroupDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPUBindGroupDescriptor bindingGroupDesc = {};
		bindingGroupDesc.label = WgpuConvert(desc.name);
		bindingGroupDesc.layout = GetSlot(gpuContext.bindingLayouts, desc.bindingLayout)->handle;

		TSpan<WGPUBindGroupEntry> bindings = PushZeroed<WGPUBindGroupEntry>(scratch.arena, desc.bindings.length);

		bindingGroupDesc.entries = bindings;
		bindingGroupDesc.entryCount = bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < bindings.length; ++bindingIdx)
		{
			WGPUBindGroupEntry& binding = bindings[bindingIdx];
			const GpuBindingGroupEntry& bindingDesc = desc.bindings[bindingIdx];

			binding.binding = bindingIdx;
			if (const GpuBuffer* buffer = GetSlot(gpuContext.buffers, bindingDesc.buffer))
			{
				BK_ASSERT(bindingDesc.bufferOffset < buffer->size);
				binding.buffer = buffer->handle;
				binding.offset = bindingDesc.bufferOffset;
				binding.size = buffer->size - bindingDesc.bufferOffset;
			}
		}

		WGPUBindGroup bindingGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &bindingGroupDesc);

		uint32 bindingGroupHandle = 0;
		if (bindingGroup)
		{
			GpuBindingGroup* bindingGroupWrapper = AcquireSlot(gpuContext.bindingGroups, &bindingGroupHandle);
			bindingGroupWrapper->handle = bindingGroup;
		}

		return bindingGroupHandle;
	}

	void DestroyBindingGroup(uint32 handle)
	{
		GpuBindingGroup* bindingGroup = GetSlot(gpuContext.bindingGroups, handle);
		if (bindingGroup)
		{
			wgpuBindGroupRelease(bindingGroup->handle);
			ReleaseSlot(gpuContext.bindingGroups, handle);
		}
	}

	uint32 CreateSurface(void* target, const GpuSurfaceDesc& desc)
	{
		WGPUSurfaceDescriptor surfaceDesc = {};

#if BK_PLATFORM_WINDOWS
		WGPUSurfaceSourceWindowsHWND surfaceSource = {};
		surfaceSource.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
		surfaceSource.hwnd = target;

		surfaceDesc.nextInChain = &surfaceSource.chain;
#elif BK_PLATFORM_MACOS
		WGPUSurfaceSourceMetalLayer surfaceSource = {};
		surfaceSource.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
		surfaceSource.layer = target;

		surfaceDesc.nextInChain = &surfaceSource.chain;
#elif BK_PLATFORM_EMSCRIPTEN
		WGPUEmscriptenSurfaceSourceCanvasHTMLSelector surfaceSource = {};
		surfaceSource.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
		surfaceSource.selector = WgpuConvert((const char*)target);

		surfaceDesc.nextInChain = &surfaceSource.chain;
#endif

		WGPUSurface surface = wgpuInstanceCreateSurface(gpuContext.instance, &surfaceDesc);

		uint32 surfaceHandle = 0;
		if (surface)
		{
			GpuSurface* surfaceWrapper = AcquireSlot(gpuContext.surfaces, &surfaceHandle);
			surfaceWrapper->handle = surface;

			ConfigureSurface(surfaceHandle, desc);
		}

		return surfaceHandle;
	}

	void ConfigureSurface(uint32 handle, const GpuSurfaceDesc& desc)
	{
		GpuSurface* surface = GetSlot(gpuContext.surfaces, handle);
		if (surface)
		{
			WGPUSurfaceCapabilities surfaceCaps = {};
			wgpuSurfaceGetCapabilities(surface->handle, gpuContext.adapter, &surfaceCaps);

			surface->config.device = gpuContext.device;
			surface->config.format = surfaceCaps.formatCount > 0 ? surfaceCaps.formats[0] : WGPUTextureFormat_Undefined;
			surface->config.usage = WGPUTextureUsage_RenderAttachment;
			surface->config.alphaMode = WGPUCompositeAlphaMode_Auto;
			surface->config.width = desc.width;
			surface->config.height = desc.height;
			surface->config.presentMode = WGPUPresentMode_Fifo;

			wgpuSurfaceConfigure(surface->handle, &surface->config);

			if (surface->depthTextureView)
			{
				wgpuTextureViewRelease(surface->depthTextureView);
				surface->depthTextureView = nullptr;
			}

			if (surface->depthTexture)
			{
				wgpuTextureRelease(surface->depthTexture);
				surface->depthTexture = nullptr;
			}

			WGPUTextureDescriptor depthTextureDesc = {};
			depthTextureDesc.usage = WGPUTextureUsage_RenderAttachment;
			depthTextureDesc.dimension = WGPUTextureDimension_2D;
			depthTextureDesc.size.width = desc.width;
			depthTextureDesc.size.height = desc.height;
			depthTextureDesc.size.depthOrArrayLayers = 1;
			depthTextureDesc.format = WGPUTextureFormat_Depth24Plus;
			depthTextureDesc.mipLevelCount = 1;
			depthTextureDesc.sampleCount = 1;
			depthTextureDesc.viewFormatCount = 1;
			depthTextureDesc.viewFormats = &depthTextureDesc.format;

			surface->depthTexture = wgpuDeviceCreateTexture(gpuContext.device, &depthTextureDesc);
			surface->depthTextureView = wgpuTextureCreateView(surface->depthTexture, nullptr);
		}
	}

	void PresentSurface(uint32 handle)
	{
#if !BK_PLATFORM_EMSCRIPTEN
		GpuSurface* surface = GetSlot(gpuContext.surfaces, handle);
		if (surface)
		{
			wgpuSurfacePresent(surface->handle);
		}
#endif
	}

	void DestroySurface(uint32 handle)
	{
		GpuSurface* surface = GetSlot(gpuContext.surfaces, handle);
		if (surface)
		{
			if (surface->depthTextureView)
			{
				wgpuTextureViewRelease(surface->depthTextureView);
				surface->depthTextureView = nullptr;
			}

			if (surface->depthTexture)
			{
				wgpuTextureRelease(surface->depthTexture);
				surface->depthTexture = nullptr;
			}

			wgpuSurfaceRelease(surface->handle);
			ReleaseSlot(gpuContext.surfaces, handle);
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

		WGPUTextureView colorTextureView = nullptr;
		WGPUTextureView depthTextureView = nullptr;

		if (GpuSurface* surface = GetSlot(gpuContext.surfaces, desc.surface))
		{
			WGPUSurfaceTexture surfaceTexture = {};
			wgpuSurfaceGetCurrentTexture(surface->handle, &surfaceTexture);

			// #TODO: Should probably release surface texture view at end of frame
			colorTextureView = wgpuTextureCreateView(surfaceTexture.texture, nullptr);
			depthTextureView = surface->depthTextureView;
		}

		WGPURenderPassColorAttachment colorAttachment = {};
		colorAttachment.view = colorTextureView;
		colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
		colorAttachment.loadOp = WGPULoadOp_Clear;
		colorAttachment.storeOp = WGPUStoreOp_Store;
		colorAttachment.clearValue.r = desc.clearColor[0];
		colorAttachment.clearValue.g = desc.clearColor[1];
		colorAttachment.clearValue.b = desc.clearColor[2];
		colorAttachment.clearValue.a = desc.clearColor[3];

		passDesc.colorAttachments = &colorAttachment;
		passDesc.colorAttachmentCount = 1;

		WGPURenderPassDepthStencilAttachment depthAttachment = {};
		depthAttachment.view = depthTextureView;
		depthAttachment.depthClearValue = 1.0f;
		depthAttachment.depthLoadOp = WGPULoadOp_Clear;
		depthAttachment.depthStoreOp = WGPUStoreOp_Store;

		passDesc.depthStencilAttachment = &depthAttachment;

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

		GpuPipeline* pipeline = GetSlot(gpuContext.pipelines, desc.pipeline);
		wgpuRenderPassEncoderSetPipeline(gpuContext.renderPassEncoder, pipeline->handle);

		for (size_t groupIdx = 0; groupIdx < desc.bindingGroups.length; ++groupIdx)
		{
			GpuBindingGroup* group = GetSlot(gpuContext.bindingGroups, desc.bindingGroups[groupIdx]);
			wgpuRenderPassEncoderSetBindGroup(gpuContext.renderPassEncoder, groupIdx, group ? group->handle : nullptr, 0, nullptr);
		}

		for (size_t bufferIdx = 0; bufferIdx < desc.vertexBuffers.length; ++bufferIdx)
		{
			GpuBuffer* buffer = GetSlot(gpuContext.buffers, desc.vertexBuffers[bufferIdx]);
			wgpuRenderPassEncoderSetVertexBuffer(gpuContext.renderPassEncoder, bufferIdx, buffer->handle, 0, buffer->size);
		}

		if (desc.indexBuffer)
		{
			GpuBuffer* buffer = GetSlot(gpuContext.buffers, desc.indexBuffer);
			wgpuRenderPassEncoderSetIndexBuffer(gpuContext.renderPassEncoder, buffer->handle, pipeline->indexFormat, 0, buffer->size);

			wgpuRenderPassEncoderDrawIndexed(
				gpuContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.indexOffset, static_cast<int32>(desc.vertexOffset), desc.instanceOffset); // #TODO: Why is baseVertex signed?
		}
		else
		{
			wgpuRenderPassEncoderDraw(
				gpuContext.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.vertexOffset, desc.instanceOffset);
		}
	}
}
