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
		.clearValue = (WGPUColor){0.8f, 0.2f, 0.3f, 1.0f},
	};

	renderPassDesc.colorAttachments = &colorAttachment;

	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);

	// TODO: ...

	wgpuRenderPassEncoderEnd(renderPass);

	WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, nullptr);
	wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

	return true;
}

void ConfigureSurface()
{
	int32_t canvasWidth, canvasHeight;
	emscripten_get_canvas_element_size("#canvas", &canvasWidth, &canvasHeight);

	gpuContext.surfaceConfig.device = gpuContext.device;
	gpuContext.surfaceConfig.format = wgpuSurfaceGetPreferredFormat(gpuContext.surface, gpuContext.adapter);
	gpuContext.surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
	gpuContext.surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
	gpuContext.surfaceConfig.width = (uint32_t)canvasWidth;
	gpuContext.surfaceConfig.height = (uint32_t)canvasHeight;
	gpuContext.surfaceConfig.presentMode = WGPUPresentMode_Fifo;

	wgpuSurfaceConfigure(gpuContext.surface, &gpuContext.surfaceConfig);
}
