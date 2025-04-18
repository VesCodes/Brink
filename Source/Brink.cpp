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

		// #TODO: temp
		uint32 pipeline;
	} appContext;

	struct GpuPipeline
	{
		WGPURenderPipeline handle;
	};

	struct GpuContext
	{
		WGPUInstance instance;
		WGPUSurface surface;
		WGPUSurfaceConfiguration surfaceConfig;
		WGPUAdapter adapter;
		WGPUDevice device;
		WGPUQueue queue;

		Pool<GpuPipeline> pipelines;
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

	uint32 CreatePipeline(const GpuPipelineDesc& desc)
	{
		WGPURenderPipelineDescriptor pipelineDesc = {};
		pipelineDesc.label = desc.name;
		pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = 0xFFFFFFFF;

		if (desc.vertexShader.code)
		{
			WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
			shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
			shaderCodeDesc.code = desc.vertexShader.code;

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderCodeDesc.chain;

			pipelineDesc.vertex.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			pipelineDesc.vertex.entryPoint = desc.vertexShader.entryPoint;
		}

		if (desc.pixelShader.code)
		{
			WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
			shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
			shaderCodeDesc.code = desc.pixelShader.code;

			WGPUShaderModuleDescriptor shaderDesc = {};
			shaderDesc.nextInChain = &shaderCodeDesc.chain;

			WGPUColorTargetState fragmentTarget = {};
			fragmentTarget.format = wgpuSurfaceGetPreferredFormat(gpuContext.surface, gpuContext.adapter);
			fragmentTarget.writeMask = WGPUColorWriteMask_All;

			WGPUFragmentState fragmentState = {};
			fragmentState.module = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
			fragmentState.entryPoint = desc.pixelShader.entryPoint;
			fragmentState.targetCount = 1;
			fragmentState.targets = &fragmentTarget;

			pipelineDesc.fragment = &fragmentState;
		}

		uint32 pipelineHandle;
		GpuPipeline* pipeline = gpuContext.pipelines.AllocateItem(&pipelineHandle);
		pipeline->handle = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

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

		appContext.pipeline = CreatePipeline({
			.name = "Test",
			.vertexShader = {
				.code = R"(
@vertex fn VsMain(@builtin(vertex_index) position: u32) -> @builtin(position) vec4f
{
	let x = f32(i32(position) - 1);
	let y = f32(i32(position & 1u) * 2 - 1);
	return vec4f(x, y, 0.0, 1.0);
}
)",
			},
			.pixelShader = {
				.code = R"(
@fragment fn PsMain() -> @location(0) vec4f
{
	return vec4f(1.0, 0.0, 0.0, 1.0);
}
)",
			},
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
		if (!gpuContext.device)
		{
			return true;
		}

		WGPUSurfaceTexture surfaceTexture;
		wgpuSurfaceGetCurrentTexture(gpuContext.surface, &surfaceTexture);

		// TODO: Check texture status

		WGPUTextureView surfaceView = wgpuTextureCreateView(surfaceTexture.texture, nullptr);

		WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(gpuContext.device, nullptr);

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

		GpuPipeline* pipeline = gpuContext.pipelines.GetItem(appContext.pipeline);
		wgpuRenderPassEncoderSetPipeline(renderPass, pipeline->handle);
		wgpuRenderPassEncoderDraw(renderPass, 3, 1, 0, 0);

		wgpuRenderPassEncoderEnd(renderPass);

		WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, nullptr);
		wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

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
	gpuContext.pipelines.Initialize(&appContext.arena, 32);

	gpuContext.instance = wgpuCreateInstance(nullptr);
	wgpuInstanceRequestAdapter(gpuContext.instance, nullptr, Bk::OnAdapterAcquired, nullptr);

	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, Bk::OnResize);
	emscripten_request_animation_frame_loop(Bk::OnFrame, nullptr);

	return 0;
}
