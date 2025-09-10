#include "Graphics.h"

#include "Core/Pool.h"

#if BK_PLATFORM_EMSCRIPTEN
#include <webgpu/webgpu.h>
#else
#include <dawn/webgpu.h>
#endif

namespace Bk
{
	struct GfxRenderPipeline
	{
		WGPURenderPipeline handle;
		WGPUIndexFormat indexFormat;
	};

	struct GfxComputePipeline
	{
		WGPUComputePipeline handle;
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

	struct GfxSurface
	{
		WGPUSurface handle;
		WGPUSurfaceConfiguration config;

		WGPUTexture depthTexture;
		WGPUTextureView depthTextureView;
	};

	struct
	{
		Arena arena;

		TPool<GfxRenderPipeline> renderPipelines;
		TPool<GfxComputePipeline> computePipelines;
		TPool<GfxBuffer> buffers;
		TPool<GfxBindingLayout> bindingLayouts;
		TPool<GfxBindingGroup> bindingGroups;
		TPool<GfxSurface> surfaces;

		WGPUInstance instance;
		WGPUAdapter adapter;
		WGPUDevice device;
		WGPUQueue queue;

		WGPUCommandEncoder commandEncoder;
		WGPURenderPassEncoder renderPassEncoder;
		WGPUComputePassEncoder computePassEncoder;
	} gfx;

	static WGPUStringView Convert(String string)
	{
		return WGPUStringView{ .data = string.data, .length = string.length != 0 ? string.length : WGPU_STRLEN };
	}

	static WGPUVertexFormat Convert(const GfxVertexFormat value)
	{
		switch (value)
		{
			case GfxVertexFormat::Uint8: return WGPUVertexFormat_Uint8;
			case GfxVertexFormat::Uint8x2: return WGPUVertexFormat_Uint8x2;
			case GfxVertexFormat::Uint8x4: return WGPUVertexFormat_Uint8x4;
			case GfxVertexFormat::Uint16: return WGPUVertexFormat_Uint16;
			case GfxVertexFormat::Uint16x2: return WGPUVertexFormat_Uint16x2;
			case GfxVertexFormat::Uint16x4: return WGPUVertexFormat_Uint16x4;
			case GfxVertexFormat::Float32: return WGPUVertexFormat_Float32;
			case GfxVertexFormat::Float32x2: return WGPUVertexFormat_Float32x2;
			case GfxVertexFormat::Float32x3: return WGPUVertexFormat_Float32x3;
			case GfxVertexFormat::Float32x4: return WGPUVertexFormat_Float32x4;
		}
	}

	static WGPUIndexFormat Convert(const GfxIndexFormat value)
	{
		switch (value)
		{
			case GfxIndexFormat::Uint16: return WGPUIndexFormat_Uint16;
			case GfxIndexFormat::Uint32: return WGPUIndexFormat_Uint32;
		}
	};

	static WGPUShaderStage Convert(const GfxBindingStage value)
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

	static WGPUBufferBindingType Convert(const GfxBindingType value)
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

			case GfxBindingType::ReadOnlyStorageBuffer:
			case GfxBindingType::DynamicReadOnlyStorageBuffer:
				return WGPUBufferBindingType_ReadOnlyStorage;
		}
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
			FatalError(1, "Failed to acquire graphics device: %.*s", static_cast<int32>(message.length), message.data);
		}

		gfx.device = device;
		BK_ASSERTF(gfx.device, "Failed to acquire graphics device");

		gfx.queue = wgpuDeviceGetQueue(gfx.device);
		BK_ASSERTF(gfx.queue, "Failed to acquire graphics device queue");
	}

	void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata1, void* userdata2)
	{
		if (status != WGPURequestAdapterStatus_Success)
		{
			FatalError(1, "Failed to acquire graphics adapter: %.*s", static_cast<int32>(message.length), message.data);
		}

		gfx.adapter = adapter;
		BK_ASSERTF(gfx.adapter, "Failed to acquire graphics adapter");

		WGPUDeviceDescriptor deviceDesc = {};
		deviceDesc.uncapturedErrorCallbackInfo.callback = OnDeviceError;
		deviceDesc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
		deviceDesc.deviceLostCallbackInfo.callback = OnDeviceLost;

#if !BK_PLATFORM_EMSCRIPTEN
		const char* enabledToggles[] = {
			"use_user_defined_labels_in_backend",
			"emit_hlsl_debug_symbols",
			"disable_symbol_renaming",
		};

		WGPUDawnTogglesDescriptor dawnTogglesDesc = {};
		dawnTogglesDesc.chain.sType = WGPUSType_DawnTogglesDescriptor;
		dawnTogglesDesc.enabledToggles = enabledToggles;
		dawnTogglesDesc.enabledToggleCount = BK_ARRAY_COUNT(enabledToggles);

		deviceDesc.nextInChain = &dawnTogglesDesc.chain;
#endif

		wgpuAdapterRequestDevice(gfx.adapter, &deviceDesc, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnDeviceAcquired });
	}

	void InitializeGraphics()
	{
		Allocate(gfx.arena, gfx.renderPipelines, 512);
		Allocate(gfx.arena, gfx.computePipelines, 512);
		Allocate(gfx.arena, gfx.buffers, 512);
		Allocate(gfx.arena, gfx.bindingLayouts, 512);
		Allocate(gfx.arena, gfx.bindingGroups, 512);
		Allocate(gfx.arena, gfx.surfaces, 32);

		gfx.instance = wgpuCreateInstance(nullptr);
		BK_ASSERTF(gfx.instance, "Failed to create graphics context");

		wgpuInstanceRequestAdapter(gfx.instance, nullptr, { .mode = WGPUCallbackMode_AllowSpontaneous, .callback = OnAdapterAcquired });
	}

	void OnShaderModuleCompiled(WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo* info, void* userdata1, void* userdata2)
	{
		// #TODO: Handle compilation issues
		BK_ASSERT(status == WGPUCompilationInfoRequestStatus_Success);
		BK_ASSERT(info->messageCount == 0);
	}

	WGPUShaderModule CreateShaderModule(String code)
	{
		WGPUShaderSourceWGSL shaderSourceDesc = {};
		shaderSourceDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
		shaderSourceDesc.code = Convert(code);

		WGPUShaderModuleDescriptor shaderDesc = {};
		shaderDesc.nextInChain = &shaderSourceDesc.chain;

		WGPUShaderModule shaderModule = wgpuDeviceCreateShaderModule(gfx.device, &shaderDesc);

		WGPUCompilationInfoCallbackInfo compilationCallback = {};
		compilationCallback.mode = WGPUCallbackMode_AllowSpontaneous;
		compilationCallback.callback = OnShaderModuleCompiled;

		wgpuShaderModuleGetCompilationInfo(shaderModule, compilationCallback);

		return shaderModule;
	}

	uint32 CreateRenderPipeline(const GfxRenderPipelineDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPURenderPipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = Convert(desc.name);
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
			pipelineDesc.vertex.module = CreateShaderModule(desc.vertexShader.code);
			pipelineDesc.vertex.entryPoint = Convert(desc.vertexShader.entryPoint);

			TSpan<WGPUVertexBufferLayout> vertexBuffers = PushZeroed<WGPUVertexBufferLayout>(scratch.arena, desc.vertexShader.buffers.length);

			pipelineDesc.vertex.buffers = vertexBuffers;
			pipelineDesc.vertex.bufferCount = vertexBuffers.length;

			uint32 attributeLocation = 0;
			for (size_t bufferIdx = 0; bufferIdx < vertexBuffers.length; ++bufferIdx)
			{
				WGPUVertexBufferLayout& buffer = vertexBuffers[bufferIdx];
				const GfxVertexBufferDesc& bufferDesc = desc.vertexShader.buffers[bufferIdx];

				TSpan<WGPUVertexAttribute> vertexAttributes = PushZeroed<WGPUVertexAttribute>(scratch.arena, bufferDesc.attributes.length);

				buffer.arrayStride = bufferDesc.stride;
				buffer.attributes = vertexAttributes;
				buffer.attributeCount = vertexAttributes.length;

				for (size_t attributeIdx = 0; attributeIdx < vertexAttributes.length; ++attributeIdx)
				{
					WGPUVertexAttribute& attribute = vertexAttributes[attributeIdx];
					const GfxVertexBufferAttribute& attributeDesc = bufferDesc.attributes[attributeIdx];

					attribute.format = Convert(attributeDesc.format);
					attribute.offset = attributeDesc.offset;
					attribute.shaderLocation = attributeLocation++;
				}
			}
		}

		if (desc.pixelShader.code.length != 0)
		{
			WGPUFragmentState fragmentState = {};
			fragmentState.module = CreateShaderModule(desc.pixelShader.code);
			fragmentState.entryPoint = Convert(desc.pixelShader.entryPoint);

			WGPUColorTargetState surfaceTarget = { .format = WGPUTextureFormat_BGRA8Unorm, .writeMask = WGPUColorWriteMask_All };
			fragmentState.targets = &surfaceTarget;
			fragmentState.targetCount = 1;

			pipelineDesc.fragment = &fragmentState;
		}

		if (desc.bindingLayouts.length != 0)
		{
			WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
			pipelineLayoutDesc.label = Convert(desc.name);

			TSpan<WGPUBindGroupLayout> bindingLayouts = PushZeroed<WGPUBindGroupLayout>(scratch.arena, desc.bindingLayouts.length);

			pipelineLayoutDesc.bindGroupLayouts = bindingLayouts;
			pipelineLayoutDesc.bindGroupLayoutCount = desc.bindingLayouts.length;

			for (size_t layoutIdx = 0; layoutIdx < bindingLayouts.length; ++layoutIdx)
			{
				bindingLayouts[layoutIdx] = GetSlot(gfx.bindingLayouts, desc.bindingLayouts[layoutIdx])->handle;
			}

			pipelineDesc.layout = wgpuDeviceCreatePipelineLayout(gfx.device, &pipelineLayoutDesc);
		}

		WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(gfx.device, &pipelineDesc);

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
			GfxRenderPipeline* pipelineWrapper = AcquireSlot(gfx.renderPipelines, &pipelineHandle);
			pipelineWrapper->handle = pipeline;
			pipelineWrapper->indexFormat = Convert(desc.indexFormat);
		}

		return pipelineHandle;
	}

	void DestroyRenderPipeline(uint32 handle)
	{
		GfxRenderPipeline* pipeline = GetSlot(gfx.renderPipelines, handle);
		if (pipeline)
		{
			wgpuRenderPipelineRelease(pipeline->handle);
			ReleaseSlot(gfx.renderPipelines, handle);
		}
	}

	uint32 CreateComputePipeline(const GfxComputePipelineDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPUComputePipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = Convert(desc.name);

		if (desc.computeShader.code.length != 0)
		{
			pipelineDesc.compute.module = CreateShaderModule(desc.computeShader.code);
			pipelineDesc.compute.entryPoint = Convert(desc.computeShader.entryPoint);
		}

		if (desc.bindingLayouts.length != 0)
		{
			WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
			pipelineLayoutDesc.label = Convert(desc.name);

			TSpan<WGPUBindGroupLayout> bindingLayouts = PushZeroed<WGPUBindGroupLayout>(scratch.arena, desc.bindingLayouts.length);

			pipelineLayoutDesc.bindGroupLayouts = bindingLayouts;
			pipelineLayoutDesc.bindGroupLayoutCount = desc.bindingLayouts.length;

			for (size_t layoutIdx = 0; layoutIdx < bindingLayouts.length; ++layoutIdx)
			{
				bindingLayouts[layoutIdx] = GetSlot(gfx.bindingLayouts, desc.bindingLayouts[layoutIdx])->handle;
			}

			pipelineDesc.layout = wgpuDeviceCreatePipelineLayout(gfx.device, &pipelineLayoutDesc);
		}

		WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(gfx.device, &pipelineDesc);

		if (pipelineDesc.compute.module)
		{
			wgpuShaderModuleRelease(pipelineDesc.compute.module);
		}

		if (pipelineDesc.layout)
		{
			wgpuPipelineLayoutRelease(pipelineDesc.layout);
		}

		uint32 pipelineHandle = 0;
		if (pipeline)
		{
			GfxComputePipeline* pipelineWrapper = AcquireSlot(gfx.computePipelines, &pipelineHandle);
			pipelineWrapper->handle = pipeline;
		}

		return pipelineHandle;
	}

	void DestroyComputePipeline(uint32 handle)
	{
		GfxComputePipeline* pipeline = GetSlot(gfx.computePipelines, handle);
		if (pipeline)
		{
			wgpuComputePipelineRelease(pipeline->handle);
			ReleaseSlot(gfx.computePipelines, handle);
		}
	}

	uint32 CreateBuffer(const GfxBufferDesc& desc)
	{
		// Allow shorthand of not specifying buffer size if passing data
		const uint64 bufferSize = desc.size == 0 ? desc.data.length : desc.size;

		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.label = Convert(desc.name);
		bufferDesc.size = AlignUp(bufferSize, 4); // Mapping requires size to be multiple of 4
		bufferDesc.mappedAtCreation = desc.data.length != 0;

		if (EnumHasAnyFlags(desc.type, GfxBufferType::Uniform))
		{
			bufferDesc.usage |= WGPUBufferUsage_Uniform;
		}

		if (EnumHasAnyFlags(desc.type, GfxBufferType::Storage))
		{
			bufferDesc.usage |= WGPUBufferUsage_Storage;
		}

		if (EnumHasAnyFlags(desc.type, GfxBufferType::Vertex))
		{
			bufferDesc.usage |= WGPUBufferUsage_Vertex;
		}

		if (EnumHasAnyFlags(desc.type, GfxBufferType::Index))
		{
			bufferDesc.usage |= WGPUBufferUsage_Index;
		}

		switch (desc.access)
		{
			case GfxBufferAccess::GpuOnly: bufferDesc.usage |= WGPUBufferUsage_CopySrc | WGPUBufferUsage_CopyDst; break;
			case GfxBufferAccess::CpuRead: bufferDesc.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst; break;
			case GfxBufferAccess::CpuWrite: bufferDesc.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc; break;
		}

		WGPUBuffer buffer = wgpuDeviceCreateBuffer(gfx.device, &bufferDesc);
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
			GfxBuffer* bufferWrapper = AcquireSlot(gfx.buffers, &bufferHandle);
			bufferWrapper->handle = buffer;
			bufferWrapper->size = bufferSize;
		}

		return bufferHandle;
	}

	size_t WriteBuffer(uint32 handle, TSpan<const uint8> data, uint64 offset)
	{
		size_t result = 0;

		GfxBuffer* buffer = GetSlot(gfx.buffers, handle);
		if (buffer)
		{
			if (data.length % 4 != 0)
			{
				ArenaScope scratch = GetScratchArena();

				TSpan<uint8> alignedData = Push(scratch.arena, AlignUp(data.length, 4));
				CopyMemory(alignedData.data, data.data, data.length);
				ZeroMemory(alignedData.data + data.length, alignedData.length - data.length);

				wgpuQueueWriteBuffer(gfx.queue, buffer->handle, offset, alignedData.data, alignedData.length);
				result = alignedData.length;
			}
			else
			{
				wgpuQueueWriteBuffer(gfx.queue, buffer->handle, offset, data.data, data.length);
				result = data.length;
			}
		}

		return result;
	}

	void DestroyBuffer(uint32 handle)
	{
		GfxBuffer* buffer = GetSlot(gfx.buffers, handle);
		if (buffer)
		{
			wgpuBufferRelease(buffer->handle);
			ReleaseSlot(gfx.buffers, handle);
		}
	}

	uint32 CreateBindingLayout(const GfxBindingLayoutDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPUBindGroupLayoutDescriptor bindingLayoutDesc = {};
		bindingLayoutDesc.label = Convert(desc.name);

		TSpan<WGPUBindGroupLayoutEntry> bindings = PushZeroed<WGPUBindGroupLayoutEntry>(scratch.arena, desc.bindings.length);

		bindingLayoutDesc.entries = bindings;
		bindingLayoutDesc.entryCount = bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < desc.bindings.length; ++bindingIdx)
		{
			WGPUBindGroupLayoutEntry& binding = bindings[bindingIdx];
			const GfxBindingLayoutEntry& bindingDesc = desc.bindings[bindingIdx];

			binding.binding = bindingIdx;
			binding.visibility = Convert(bindingDesc.stage);
			binding.bindingArraySize = 1; // #TODO: https://github.com/gpuweb/gpuweb/blob/main/proposals/sized-binding-arrays.md
			binding.buffer.type = Convert(bindingDesc.type);
			binding.buffer.hasDynamicOffset = (bindingDesc.type == GfxBindingType::DynamicUniformBuffer || bindingDesc.type == GfxBindingType::DynamicStorageBuffer || bindingDesc.type == GfxBindingType::DynamicReadOnlyStorageBuffer);
		}

		WGPUBindGroupLayout bindingLayout = wgpuDeviceCreateBindGroupLayout(gfx.device, &bindingLayoutDesc);

		uint32 bindingLayoutHandle = 0;
		if (bindingLayout)
		{
			GfxBindingLayout* bindingLayoutWrapper = AcquireSlot(gfx.bindingLayouts, &bindingLayoutHandle);
			bindingLayoutWrapper->handle = bindingLayout;
		}

		return bindingLayoutHandle;
	}

	void DestroyBindingLayout(uint32 handle)
	{
		GfxBindingLayout* bindingLayout = GetSlot(gfx.bindingLayouts, handle);
		if (bindingLayout)
		{
			wgpuBindGroupLayoutRelease(bindingLayout->handle);
			ReleaseSlot(gfx.bindingLayouts, handle);
		}
	}

	uint32 CreateBindingGroup(const GfxBindingGroupDesc& desc)
	{
		ArenaScope scratch = GetScratchArena();

		WGPUBindGroupDescriptor bindingGroupDesc = {};
		bindingGroupDesc.label = Convert(desc.name);
		bindingGroupDesc.layout = GetSlot(gfx.bindingLayouts, desc.bindingLayout)->handle;

		TSpan<WGPUBindGroupEntry> bindings = PushZeroed<WGPUBindGroupEntry>(scratch.arena, desc.bindings.length);

		bindingGroupDesc.entries = bindings;
		bindingGroupDesc.entryCount = bindings.length;

		for (size_t bindingIdx = 0; bindingIdx < bindings.length; ++bindingIdx)
		{
			WGPUBindGroupEntry& binding = bindings[bindingIdx];
			const GfxBindingGroupEntry& bindingDesc = desc.bindings[bindingIdx];

			binding.binding = bindingIdx;
			if (const GfxBuffer* buffer = GetSlot(gfx.buffers, bindingDesc.buffer))
			{
				BK_ASSERT(bindingDesc.offset + bindingDesc.size <= buffer->size);
				binding.buffer = buffer->handle;
				binding.offset = bindingDesc.offset;
				binding.size = bindingDesc.size != 0 ? bindingDesc.size : buffer->size - bindingDesc.offset;
			}
		}

		WGPUBindGroup bindingGroup = wgpuDeviceCreateBindGroup(gfx.device, &bindingGroupDesc);

		uint32 bindingGroupHandle = 0;
		if (bindingGroup)
		{
			GfxBindingGroup* bindingGroupWrapper = AcquireSlot(gfx.bindingGroups, &bindingGroupHandle);
			bindingGroupWrapper->handle = bindingGroup;
		}

		return bindingGroupHandle;
	}

	void DestroyBindingGroup(uint32 handle)
	{
		GfxBindingGroup* bindingGroup = GetSlot(gfx.bindingGroups, handle);
		if (bindingGroup)
		{
			wgpuBindGroupRelease(bindingGroup->handle);
			ReleaseSlot(gfx.bindingGroups, handle);
		}
	}

	uint32 CreateSurface(void* target, const GfxSurfaceDesc& desc)
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
		surfaceSource.selector = Convert((const char*)target);

		surfaceDesc.nextInChain = &surfaceSource.chain;
#endif

		WGPUSurface surface = wgpuInstanceCreateSurface(gfx.instance, &surfaceDesc);

		uint32 surfaceHandle = 0;
		if (surface)
		{
			GfxSurface* surfaceWrapper = AcquireSlot(gfx.surfaces, &surfaceHandle);
			surfaceWrapper->handle = surface;

			ConfigureSurface(surfaceHandle, desc);
		}

		return surfaceHandle;
	}

	void ConfigureSurface(uint32 handle, const GfxSurfaceDesc& desc)
	{
		GfxSurface* surface = GetSlot(gfx.surfaces, handle);
		if (surface)
		{
			WGPUSurfaceCapabilities surfaceCaps = {};
			wgpuSurfaceGetCapabilities(surface->handle, gfx.adapter, &surfaceCaps);

			surface->config.device = gfx.device;
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

			surface->depthTexture = wgpuDeviceCreateTexture(gfx.device, &depthTextureDesc);
			surface->depthTextureView = wgpuTextureCreateView(surface->depthTexture, nullptr);
		}
	}

	void PresentSurface(uint32 handle)
	{
#if !BK_PLATFORM_EMSCRIPTEN
		GfxSurface* surface = GetSlot(gfx.surfaces, handle);
		if (surface)
		{
			wgpuSurfacePresent(surface->handle);
		}
#endif
	}

	void DestroySurface(uint32 handle)
	{
		GfxSurface* surface = GetSlot(gfx.surfaces, handle);
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
			ReleaseSlot(gfx.surfaces, handle);
		}
	}

	bool BeginFrame()
	{
		if (!gfx.device)
		{
			return false;
		}

		gfx.commandEncoder = wgpuDeviceCreateCommandEncoder(gfx.device, nullptr);

		return true;
	}

	bool EndFrame()
	{
		if (!gfx.commandEncoder)
		{
			return false;
		}

		WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(gfx.commandEncoder, nullptr);
		wgpuQueueSubmit(gfx.queue, 1, &commandBuffer);
		wgpuCommandBufferRelease(commandBuffer);

		wgpuCommandEncoderRelease(gfx.commandEncoder);
		gfx.commandEncoder = nullptr;

		return true;
	}

	void BeginRenderPass(const GfxRenderPassDesc& desc)
	{
		BK_ASSERT(gfx.renderPassEncoder == nullptr);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.label = Convert(desc.name);

		WGPUTextureView colorTextureView = nullptr;
		WGPUTextureView depthTextureView = nullptr;

		if (GfxSurface* surface = GetSlot(gfx.surfaces, desc.surface))
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

		wgpuCommandEncoderPushDebugGroup(gfx.commandEncoder, Convert(desc.name));
		gfx.renderPassEncoder = wgpuCommandEncoderBeginRenderPass(gfx.commandEncoder, &passDesc);
	}

	void EndRenderPass()
	{
		BK_ASSERT(gfx.renderPassEncoder != nullptr);

		wgpuRenderPassEncoderEnd(gfx.renderPassEncoder);
		wgpuRenderPassEncoderRelease(gfx.renderPassEncoder);
		gfx.renderPassEncoder = nullptr;

		wgpuCommandEncoderPopDebugGroup(gfx.commandEncoder);
	}

	void BeginComputePass(const GfxComputePassDesc& desc)
	{
		BK_ASSERT(gfx.computePassEncoder == nullptr);

		WGPUComputePassDescriptor passDesc = {};
		passDesc.label = Convert(desc.name);

		wgpuCommandEncoderPushDebugGroup(gfx.commandEncoder, Convert(desc.name));
		gfx.computePassEncoder = wgpuCommandEncoderBeginComputePass(gfx.commandEncoder, &passDesc);
	}

	void EndComputePass()
	{
		BK_ASSERT(gfx.computePassEncoder != nullptr);

		wgpuComputePassEncoderEnd(gfx.computePassEncoder);
		wgpuComputePassEncoderRelease(gfx.computePassEncoder);
		gfx.computePassEncoder = nullptr;

		wgpuCommandEncoderPopDebugGroup(gfx.commandEncoder);
	}

	void Draw(const GfxDrawDesc& desc)
	{
		BK_ASSERT(gfx.renderPassEncoder != nullptr);
		BK_ASSERT(desc.pipeline);

		GfxRenderPipeline* pipeline = GetSlot(gfx.renderPipelines, desc.pipeline);
		wgpuRenderPassEncoderSetPipeline(gfx.renderPassEncoder, pipeline->handle);

		for (size_t groupIdx = 0; groupIdx < desc.bindingGroups.length; ++groupIdx)
		{
			GfxBindingGroup* group = GetSlot(gfx.bindingGroups, desc.bindingGroups[groupIdx].bindingGroup);
			TSpan<uint32> dynamicOffsets = desc.bindingGroups[groupIdx].dynamicOffsets;

			wgpuRenderPassEncoderSetBindGroup(gfx.renderPassEncoder, groupIdx, group ? group->handle : nullptr, dynamicOffsets.length, dynamicOffsets.data);
		}

		for (size_t bufferIdx = 0; bufferIdx < desc.vertexBuffers.length; ++bufferIdx)
		{
			GfxBuffer* buffer = GetSlot(gfx.buffers, desc.vertexBuffers[bufferIdx]);
			wgpuRenderPassEncoderSetVertexBuffer(gfx.renderPassEncoder, bufferIdx, buffer->handle, 0, buffer->size);
		}

		if (desc.indexBuffer)
		{
			GfxBuffer* buffer = GetSlot(gfx.buffers, desc.indexBuffer);
			wgpuRenderPassEncoderSetIndexBuffer(gfx.renderPassEncoder, buffer->handle, pipeline->indexFormat, 0, buffer->size);

			wgpuRenderPassEncoderDrawIndexed(
				gfx.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.indexOffset, static_cast<int32>(desc.vertexOffset), desc.instanceOffset); // #TODO: Why is baseVertex signed?
		}
		else
		{
			wgpuRenderPassEncoderDraw(
				gfx.renderPassEncoder, desc.triangleCount * 3, desc.instanceCount,
				desc.vertexOffset, desc.instanceOffset);
		}
	}

	void Dispatch(const GfxDispatchDesc& desc)
	{
		BK_ASSERT(gfx.computePassEncoder != nullptr);
		BK_ASSERT(desc.pipeline);

		GfxComputePipeline* pipeline = GetSlot(gfx.computePipelines, desc.pipeline);
		wgpuComputePassEncoderSetPipeline(gfx.computePassEncoder, pipeline->handle);

		for (size_t groupIdx = 0; groupIdx < desc.bindingGroups.length; ++groupIdx)
		{
			GfxBindingGroup* group = GetSlot(gfx.bindingGroups, desc.bindingGroups[groupIdx].bindingGroup);
			TSpan<uint32> dynamicOffsets = desc.bindingGroups[groupIdx].dynamicOffsets;

			wgpuComputePassEncoderSetBindGroup(gfx.computePassEncoder, groupIdx, group ? group->handle : nullptr, dynamicOffsets.length, dynamicOffsets.data);
		}

		wgpuComputePassEncoderDispatchWorkgroups(gfx.computePassEncoder, desc.workgroupCountX, desc.workgroupCountY, desc.workgroupCountZ);
	}
}
