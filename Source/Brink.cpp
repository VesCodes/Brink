#include "Brink.h"

#include "BkCore/BkPool.h"

#include "BkCore/BkCore.cpp"
#include "BkCore/BkMemory.cpp"
#include "BkCore/BkString.cpp"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <webgpu/webgpu.h>

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

		TPool<GpuPipeline> pipelines;
		TPool<GpuBuffer> buffers;
		TPool<GpuBindingLayout> bindingLayouts;
		TPool<GpuBindingGroup> bindingGroups;
	} gpuContext;

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

	static WGPUShaderStageFlags WgpuConvert(const GpuBindingStage value)
	{
		WGPUShaderStageFlags result = WGPUShaderStage_None;

		if (value == GpuBindingStage::Vertex || value == GpuBindingStage::All)
		{
			result |= WGPUShaderStage_Vertex;
		}

		if (value == GpuBindingStage::Pixel || value == GpuBindingStage::All)
		{
			result |= WGPUShaderStage_Fragment;
		}

		if (value == GpuBindingStage::Compute || value == GpuBindingStage::All)
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

		if (desc.bindingLayouts.length > 0)
		{
			WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
			pipelineLayoutDesc.label = desc.name;

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
			if (pipeline->handle)
			{
				wgpuRenderPipelineRelease(pipeline->handle);
			}

			gpuContext.pipelines.FreeItem(handle);
		}
	}

	uint32 CreateBuffer(const GpuBufferDesc& desc)
	{
		// Allow shorthand of not specifying buffer size if passing data
		const uint64 bufferSize = desc.size == 0 ? desc.data.length : desc.size;

		WGPUBufferDescriptor bufferDesc = {};
		bufferDesc.label = desc.name;
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

	void DestroyBuffer(uint32 handle)
	{
		GpuBuffer* buffer = gpuContext.buffers.GetItem(handle);
		if (buffer)
		{
			if (buffer->handle)
			{
				wgpuBufferRelease(buffer->handle);
			}

			gpuContext.buffers.FreeItem(handle);
		}
	}

	uint32 CreateBindingLayout(const GpuBindingLayoutDesc& desc)
	{
		WGPUBindGroupLayoutDescriptor bindingLayoutDesc = {};
		bindingLayoutDesc.label = desc.name;

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
			if (bindingLayout->handle)
			{
				wgpuBindGroupLayoutRelease(bindingLayout->handle);
			}

			gpuContext.bindingLayouts.FreeItem(handle);
		}
	}

	uint32 CreateBindingGroup(const GpuBindingGroupDesc& desc)
	{
		WGPUBindGroupDescriptor bindingGroupDesc = {};
		bindingGroupDesc.label = desc.name;
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
			if (bindingGroup->handle)
			{
				wgpuBindGroupRelease(bindingGroup->handle);
			}

			gpuContext.bindingGroups.FreeItem(handle);
		}
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
	gpuContext.bindingLayouts.Initialize(&appContext.arena, 32);
	gpuContext.bindingGroups.Initialize(&appContext.arena, 32);

	gpuContext.instance = wgpuCreateInstance(nullptr);
	wgpuInstanceRequestAdapter(gpuContext.instance, nullptr, Bk::OnAdapterAcquired, nullptr);

	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, Bk::OnResize);
	emscripten_request_animation_frame_loop(Bk::OnFrame, nullptr);

	return 0;
}
