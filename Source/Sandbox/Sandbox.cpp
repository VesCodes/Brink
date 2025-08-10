#include "Core/Core.h"
#include "Core/Gpu.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "GltfLoader.h"

#define HANDMADE_MATH_USE_DEGREES
#include <HandmadeMath.h>

#include <emscripten/emscripten.h>

using namespace Bk;

struct
{
	bool initialized;

	Arena arena;

	uint32 testPipeline;
	uint32 testVertexBuffer;
	uint32 testIndexBuffer;
	uint32 testUniformBuffer;
	uint32 testBindingGroup;
	uint32 testTriangleCount;
} state;

const char* testShader = R"(
@group(0) @binding(0) var<uniform> mvp: mat4x4f;

struct VsInput
{
	@location(0) position: vec3f,
}

struct VsOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
}

@vertex fn VsMain(input: VsInput) -> VsOutput {
	var output: VsOutput;
	output.position = mvp * vec4(input.position, 1);
	output.color = vec4(0.5, 0.675, 0.6, 1.0);
	return output;
}

struct PsInput
{
	@location(0) color: vec4f,
}

@fragment fn PsMain(input: PsInput) -> @location(0) vec4f {
	return input.color;
}
)";

void Initialize()
{
	uint32 testBindingLayout = CreateBindingLayout({
		.name = "Test Binding Layout",
		.bindings = {
			{ .type = GpuBindingType::UniformBuffer, .stage = GpuBindingStage::Vertex },
		},
	});

	state.testPipeline = CreatePipeline({
		.name = "Test Pipeline",
		.vertexShader = {
			.code = testShader,
			.buffers = {
				{
					.stride = 12,
					.attributes = {
						{ .offset = 0, .format = GpuVertexFormat::Float32x3 },
					},
				},
			},
		},
		.pixelShader = {
			.code = testShader,
		},
		.bindingLayouts = {
			testBindingLayout,
		},
	});

	TSpan<Mesh> meshes;
	LoadGltfMeshes(state.arena, "Assets/Knight.glb", meshes);

	state.testVertexBuffer = CreateBuffer({
		.name = "Test Vertex Buffer",
		.type = GpuBufferType::Vertex,
		.data = meshes[1].sections[0].positionBuffer,
	});

	state.testIndexBuffer = CreateBuffer({
		.name = "Test Index Buffer",
		.type = GpuBufferType::Index,
		.data = meshes[1].sections[0].indexBuffer,
	});

	state.testTriangleCount = meshes[1].sections[0].indexBuffer.length / sizeof(uint16) / 3;

	state.testUniformBuffer = CreateBuffer({
		.name = "Test Uniform Buffer",
		.type = GpuBufferType::Uniform,
		.access = GpuBufferAccess::GpuOnly,
		.size = sizeof(HMM_Mat4),
	});

	state.testBindingGroup = CreateBindingGroup({
		.name = "Test Binding Group",
		.bindingLayout = testBindingLayout,
		.bindings = {
			{ .buffer = state.testUniformBuffer },
		},
	});
}

void Update()
{
	if (!BeginFrame())
	{
		return;
	}

	if (!state.initialized)
	{
		Initialize();
		state.initialized = true;
	}

	float t = static_cast<float>(GetTimeSec());

	HMM_Mat4 proj = HMM_Perspective_RH_ZO(75, 16.0f / 9.0f, 0.01f, 100);
	HMM_Mat4 view = HMM_LookAt_RH(HMM_V3(0, 2, 6), HMM_V3(0, 0, 0), HMM_V3(0, 1, 0));
	HMM_Mat4 model = HMM_Translate(HMM_V3(0, HMM_SinF(t * 16 * HMM_PI32), 0)) * HMM_Rotate_RH(t * 32, HMM_V3(0, 1, 0));

	HMM_Mat4 mvp = proj * view * model;

	WriteBuffer(state.testUniformBuffer, TSpan((uint8*)mvp.Elements, sizeof(mvp)));

	BeginPass({
		.name = "Sandbox Pass",
		.clearColor = { 0.2f, 0.2f, 0.3f, 1.0f },
	});

	Draw({
		.pipeline = state.testPipeline,
		.vertexBuffer = state.testVertexBuffer,
		.indexBuffer = state.testIndexBuffer,
		.bindingGroups = {
			state.testBindingGroup,
		},
		.triangleCount = state.testTriangleCount,
		.instanceCount = 1,
	});

	EndPass();

	EndFrame();
}

int main(int argc, char** argv)
{
	GpuInitialize();

	emscripten_set_main_loop(Update, 0, true);

	return 0;
}
