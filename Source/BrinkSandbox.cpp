#include "BkCore/BkArena.cpp"
#include "BkCore/BkCore.cpp"
#include "BkCore/BkFile.cpp"
#include "BkCore/BkGpu.cpp"
#include "BkCore/BkMemory.cpp"
#include "BkCore/BkString.cpp"

#define HANDMADE_MATH_USE_DEGREES
#include "../ThirdParty/HandmadeMath.h"

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
} state;

const char* testShader = R"(
@group(0) @binding(0) var<uniform> mvp: mat4x4f;

struct VsInput
{
	@location(0) position: vec4f,
	@location(1) color: vec4f,
}

struct VsOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec4f,
}

@vertex fn VsMain(input: VsInput) -> VsOutput {
	var output: VsOutput;
	output.position = mvp * input.position;
	output.color = input.color;
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

// clang-format off
float vertices[] = {
	-1.0, -1.0, -1.0,	1.0, 0.0, 0.0, 1.0,
	 1.0, -1.0, -1.0,	1.0, 0.0, 0.0, 1.0,
	 1.0,  1.0, -1.0,	1.0, 0.0, 0.0, 1.0,
	-1.0,  1.0, -1.0,	1.0, 0.0, 0.0, 1.0,

	-1.0, -1.0,  1.0,	0.0, 1.0, 0.0, 1.0,
	 1.0, -1.0,  1.0,	0.0, 1.0, 0.0, 1.0,
	 1.0,  1.0,  1.0,	0.0, 1.0, 0.0, 1.0,
	-1.0,  1.0,  1.0,	0.0, 1.0, 0.0, 1.0,

	-1.0, -1.0, -1.0,	0.0, 0.0, 1.0, 1.0,
	-1.0,  1.0, -1.0,	0.0, 0.0, 1.0, 1.0,
	-1.0,  1.0,  1.0,	0.0, 0.0, 1.0, 1.0,
	-1.0, -1.0,  1.0,	0.0, 0.0, 1.0, 1.0,

	1.0, -1.0, -1.0,	1.0, 0.5, 0.0, 1.0,
	1.0,  1.0, -1.0,	1.0, 0.5, 0.0, 1.0,
	1.0,  1.0,  1.0,	1.0, 0.5, 0.0, 1.0,
	1.0, -1.0,  1.0,	1.0, 0.5, 0.0, 1.0,

	-1.0, -1.0, -1.0,	0.0, 0.5, 1.0, 1.0,
	-1.0, -1.0,  1.0,	0.0, 0.5, 1.0, 1.0,
	 1.0, -1.0,  1.0,	0.0, 0.5, 1.0, 1.0,
	 1.0, -1.0, -1.0,	0.0, 0.5, 1.0, 1.0,

	-1.0,  1.0, -1.0,	1.0, 0.0, 0.5, 1.0,
	-1.0,  1.0,  1.0,	1.0, 0.0, 0.5, 1.0,
	 1.0,  1.0,  1.0,	1.0, 0.0, 0.5, 1.0,
	 1.0,  1.0, -1.0,	1.0, 0.0, 0.5, 1.0
};

uint16 indices[] = {
	0,	1,	2,	0,	2,	3,
	6,	5,	4,	7,	6,	4,
	8,	9,	10,	8,	10, 11,
	14, 13, 12,	15, 14, 12,
	16, 17, 18,	16, 18, 19,
	22, 21, 20,	23, 22, 20
};
// clang-format on

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
		.VS = {
			.code = testShader,
			.buffers = {
				{
					.stride = 28,
					.attributes = {
						{ .offset = 0, .format = GpuVertexFormat::Float32x3 },
						{ .offset = 12, .format = GpuVertexFormat::Float32x4 },
					},
				},
			},
		},
		.PS = {
			.code = testShader,
		},
		.bindingLayouts = {
			testBindingLayout,
		},
	});

	state.testVertexBuffer = CreateBuffer({
		.name = "Test Vertex Buffer",
		.type = GpuBufferType::Vertex,
		.data = TSpan((uint8*)vertices, sizeof(vertices)),
	});

	state.testIndexBuffer = CreateBuffer({
		.name = "Test Index Buffer",
		.type = GpuBufferType::Index,
		.data = TSpan((uint8*)indices, sizeof(indices)),
	});

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
		.triangleCount = BK_ARRAY_COUNT(indices) / 3,
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
