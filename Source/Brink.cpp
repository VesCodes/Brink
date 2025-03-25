#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <webgpu/webgpu.h>

#include <stdio.h>

struct GpuContext
{
	WGPUInstance instance;
	WGPUSurface surface;
	WGPUSurfaceConfiguration surfaceConfig;
	WGPUAdapter adapter;
	WGPUDevice device;
	WGPUQueue queue;

	WGPURenderPipeline renderPipeline;
} gpuContext;

void OnAdapterAcquired(WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userData);
void OnDeviceAcquired(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userData);
void OnDeviceError(WGPUErrorType type, const char* message, void* userData);
bool OnResize(int eventType, const EmscriptenUiEvent* event, void* userData);
bool OnFrame(double time, void* userData);
void ConfigureSurface();

int main(int ArgC, char** ArgV)
{
	gpuContext.instance = wgpuCreateInstance(nullptr);
	wgpuInstanceRequestAdapter(gpuContext.instance, nullptr, OnAdapterAcquired, nullptr);

	emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, false, OnResize);
	emscripten_request_animation_frame_loop(OnFrame, nullptr);

	return 0;
}

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

	{
		WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {};
		shaderCodeDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		shaderCodeDesc.code = R"(
			@vertex
			fn VsMain(@builtin(vertex_index) position: u32) -> @builtin(position) vec4f {
				let x = f32(i32(position) - 1);
				let y = f32(i32(position & 1u) * 2 - 1);
				return vec4f(x, y, 0.0, 1.0);
			}

			@fragment
			fn FsMain() -> @location(0) vec4f {
				return vec4f(1.0, 0.0, 0.0, 1.0);
			}
		)";

		WGPUShaderModuleDescriptor shaderDesc = {};
		shaderDesc.nextInChain = &shaderCodeDesc.chain;

		WGPUShaderModule shader = wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);

		WGPUColorTargetState fragmentTarget = {};
		fragmentTarget.format = wgpuSurfaceGetPreferredFormat(gpuContext.surface, gpuContext.adapter);
		fragmentTarget.writeMask = WGPUColorWriteMask_All;

		WGPUFragmentState fragmentState = {};
		fragmentState.module = shader;
		fragmentState.entryPoint = "FsMain";
		fragmentState.targetCount = 1;
		fragmentState.targets = &fragmentTarget;

		WGPURenderPipelineDescriptor renderPipelineDesc = {};
		renderPipelineDesc.vertex.module = shader;
		renderPipelineDesc.vertex.entryPoint = "VsMain";
		renderPipelineDesc.fragment = &fragmentState;
		renderPipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		renderPipelineDesc.multisample.count = 1;
		renderPipelineDesc.multisample.mask = 0xFFFFFFFF;

		gpuContext.renderPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &renderPipelineDesc);
		printf("Created render pipeline: %p\n", gpuContext.renderPipeline);
	}
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

	wgpuRenderPassEncoderSetPipeline(renderPass, gpuContext.renderPipeline);
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
