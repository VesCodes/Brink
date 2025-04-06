#include "Brink.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <webgpu/webgpu.h>

#include <stdio.h>
#include <string.h>

namespace Bk
{
	struct Shader
	{
		WGPUShaderModule wgpuHandle;
		const char* entrypoint;
	};

	struct Pipeline
	{
		WGPURenderPipeline wgpuHandle;
	};

	struct AppContext
	{
		Arena arena;

		struct
		{
			WGPUInstance instance;
			WGPUSurface surface;
			WGPUSurfaceConfiguration surfaceConfig;
			WGPUAdapter adapter;
			WGPUDevice device;
			WGPUQueue queue;

			Pool<Shader> shaders;
			Pool<Pipeline> pipelines;
		} gpu;

		// #TODO: temp
		uint32 pipeline;
	} context;

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

	uint32 CreateShader(const ShaderDesc& desc)
	{
		WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
		shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		shaderCodeDesc.code = desc.code;

		WGPUShaderModuleDescriptor shaderDesc = {};
		shaderDesc.nextInChain = &shaderCodeDesc.chain;

		uint32 shaderHandle;
		Shader* shader = context.gpu.shaders.AllocateItem(&shaderHandle);
		shader->wgpuHandle = wgpuDeviceCreateShaderModule(context.gpu.device, &shaderDesc);

		printf("createshader(%s) = %p\n", desc.entrypoint, shader->wgpuHandle);

		return shaderHandle;
	}

	uint32 CreatePipeline(const PipelineDesc& desc)
	{
		Shader* vertexShader = context.gpu.shaders.GetItem(desc.vertexShader);
		Shader* pixelShader = context.gpu.shaders.GetItem(desc.pixelShader);

		WGPUColorTargetState fragmentTarget = {};
		fragmentTarget.format = wgpuSurfaceGetPreferredFormat(context.gpu.surface, context.gpu.adapter);
		fragmentTarget.writeMask = WGPUColorWriteMask_All;

		WGPUFragmentState fragmentState = {};
		fragmentState.module = pixelShader->wgpuHandle;
		fragmentState.entryPoint = pixelShader->entrypoint;
		fragmentState.targetCount = 1;
		fragmentState.targets = &fragmentTarget;

		WGPURenderPipelineDescriptor renderPipelineDesc = {};
		renderPipelineDesc.vertex.module = vertexShader->wgpuHandle;
		renderPipelineDesc.vertex.entryPoint = vertexShader->entrypoint;
		renderPipelineDesc.fragment = &fragmentState;
		renderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		renderPipelineDesc.multisample.count = 1;
		renderPipelineDesc.multisample.mask = 0xFFFFFFFF;

		uint32 pipelineHandle;
		Pipeline* pipeline = context.gpu.pipelines.AllocateItem(&pipelineHandle);
		pipeline->wgpuHandle = wgpuDeviceCreateRenderPipeline(context.gpu.device, &renderPipelineDesc);

		return pipelineHandle;
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

		context.gpu.adapter = adapter;

		wgpuAdapterRequestDevice(adapter, nullptr, OnDeviceAcquired, nullptr);
	}

	void OnDeviceAcquired(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userData)
	{
		printf("Device acquired: %p\n", device);

		wgpuDeviceSetUncapturedErrorCallback(device, OnDeviceError, nullptr);

		context.gpu.device = device;
		context.gpu.queue = wgpuDeviceGetQueue(context.gpu.device);

		WGPUSurfaceDescriptorFromCanvasHTMLSelector canvasDesc = {};
		canvasDesc.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
		canvasDesc.selector = "#canvas";

		WGPUSurfaceDescriptor surfaceDesc = {};
		surfaceDesc.nextInChain = &canvasDesc.chain;
		context.gpu.surface = wgpuInstanceCreateSurface(context.gpu.instance, &surfaceDesc);

		ConfigureSurface();

		// #TODO: temp
		uint32 vertexShader = CreateShader({
			.type = ShaderType::Vertex,
			.entrypoint = "VsMain",
			.code = R"(
@vertex
fn VsMain(@builtin(vertex_index) position: u32) -> @builtin(position) vec4f
{
let x = f32(i32(position) - 1);
let y = f32(i32(position & 1u) * 2 - 1);
return vec4f(x, y, 0.0, 1.0);
}
)",
		});

		uint32 pixelShader = CreateShader({
			.type = ShaderType::Pixel,
			.entrypoint = "PsMain",
			.code = R"(
@fragment
fn PsMain() -> @location(0) vec4f
{
return vec4f(1.0, 0.0, 0.0, 1.0);
}
)",
		});

		context.pipeline = CreatePipeline({
			.vertexShader = vertexShader,
			.pixelShader = pixelShader,
		});
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
		if (!context.gpu.device)
		{
			return true;
		}

		WGPUSurfaceTexture surfaceTexture;
		wgpuSurfaceGetCurrentTexture(context.gpu.surface, &surfaceTexture);

		// TODO: Check texture status

		WGPUTextureView surfaceView = wgpuTextureCreateView(surfaceTexture.texture, nullptr);

		WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(context.gpu.device, nullptr);

		WGPURenderPassDescriptor renderPassDesc = {};
		renderPassDesc.colorAttachmentCount = 1;
		WGPURenderPassColorAttachment colorAttachment = {
			.view = surfaceView,
			.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
			.loadOp = WGPULoadOp_Clear,
			.storeOp = WGPUStoreOp_Store,
			.clearValue = (WGPUColor){0.0f, 0.2f, 0.3f, 1.0f},
		};

		renderPassDesc.colorAttachments = &colorAttachment;

		WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);

		Pipeline* pipeline = context.gpu.pipelines.GetItem(context.pipeline);
		wgpuRenderPassEncoderSetPipeline(renderPass, pipeline->wgpuHandle);
		wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

		wgpuRenderPassEncoderEnd(renderPass);

		WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, nullptr);
		wgpuQueueSubmit(context.gpu.queue, 1, &cmdBuffer);

		return true;
	}

	void ConfigureSurface()
	{
		double canvasWidth, canvasHeight;
		emscripten_get_element_css_size("#canvas", &canvasWidth, &canvasHeight);

		context.gpu.surfaceConfig.device = context.gpu.device;
		context.gpu.surfaceConfig.format = wgpuSurfaceGetPreferredFormat(context.gpu.surface, context.gpu.adapter);
		context.gpu.surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
		context.gpu.surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
		context.gpu.surfaceConfig.width = (uint32_t)canvasWidth;
		context.gpu.surfaceConfig.height = (uint32_t)canvasHeight;
		context.gpu.surfaceConfig.presentMode = WGPUPresentMode_Fifo;

		wgpuSurfaceConfigure(context.gpu.surface, &context.gpu.surfaceConfig);

		printf("Configured surface: (%d x %d)\n", context.gpu.surfaceConfig.width, context.gpu.surfaceConfig.height);
	}
} // namespace Bk

int main(int argc, char** argv)
{
	using namespace Bk;

	context.arena.Initialize(BK_MEGABYTES(8));

	context.gpu.shaders.Initialize(&context.arena, 32);
	context.gpu.pipelines.Initialize(&context.arena, 32);

	context.gpu.instance = wgpuCreateInstance(nullptr);
	wgpuInstanceRequestAdapter(context.gpu.instance, nullptr, Bk::OnAdapterAcquired, nullptr);

	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, Bk::OnResize);
	emscripten_request_animation_frame_loop(Bk::OnFrame, nullptr);

	return 0;
}
