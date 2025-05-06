#include <math.h>

#include "BkCore/BkCore.cpp"
#include "BkCore/BkGraphics.cpp"
#include "BkCore/BkMemory.cpp"
#include "BkCore/BkString.cpp"

using namespace Bk;

struct
{
	bool initialized;

	Arena arena;

	uint32 testPipeline;
	uint32 testVertexBuffer;
	uint32 testIndexBuffer;
	uint32 testBindingGroup;
} state;

const char* testShader = R"(
struct Globals
{
	color: vec4f
};

@group(0) @binding(0) var<uniform> globals : Globals;
@vertex fn VsMain(@location(0) position: vec2f) -> @builtin(position) vec4f
{
    return vec4f(position, 0.0, 1.0);
}

@fragment fn PsMain() -> @location(0) vec4f
{
    return globals.color;
}
)";

uint32 subdivisions = 64;
uint32 numVertices = (subdivisions + 1) * 2;
uint32 numIndices = subdivisions * 3;

void Initialize()
{
	state.arena.Initialize(BK_MEGABYTES(2));

	float globals[] = { 0.2f, 0.7f, 0.3f, 0.0f };

	uint32 testBindingLayout = CreateBindingLayout({
		.name = "Test Binding Layout",
		.bindings = {
			{ .type = GfxBindingType::UniformBuffer, .stage = GfxBindingStage::Vertex | GfxBindingStage::Pixel },
		},
	});

	state.testPipeline = CreatePipeline({
		.name = "Test Pipeline",
		.VS = {
			.code = testShader,
			.buffers = {
				{
					.stride = 8,
					.attributes = {
						{ .offset = 0, .format = GfxVertexFormat::Float32x2 },
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

	float* vertices = state.arena.Push<float>(numVertices);
	uint16* indices = state.arena.Push<uint16>(numIndices);

	float radius = 0.75f;

	vertices[0] = vertices[1] = 0;
	for (uint32 i = 0; i < subdivisions; ++i)
	{
		float angle = float(i) / float(subdivisions) * 2.0f * 3.14159265f;
		vertices[(i + 1) * 2 + 0] = cosf(angle) * radius;
		vertices[(i + 1) * 2 + 1] = sinf(angle) * radius;
		indices[(i * 3) + 0] = 0;
		indices[(i * 3) + 1] = i + 1;
		indices[(i * 3) + 2] = (i + 1) % subdivisions + 1;
	}

	state.testVertexBuffer = CreateBuffer({
		.name = "Test Vertex Buffer",
		.type = GfxBufferType::Vertex,
		.data = TSpan((uint8*)vertices, numVertices * sizeof(vertices[0])),
	});

	state.testIndexBuffer = CreateBuffer({
		.name = "Test Index Buffer",
		.type = GfxBufferType::Index,
		.data = TSpan((uint8*)indices, numIndices * sizeof(indices[0])),
	});

	uint32 testGlobalsBuffer = CreateBuffer({
		.name = "Test Globals Buffer",
		.type = GfxBufferType::Uniform,
		.access = GfxBufferAccess::GpuOnly,
		.data = TSpan((uint8*)globals, sizeof(globals)),
	});

	state.testBindingGroup = CreateBindingGroup({
		.name = "Test Binding Group",
		.bindingLayout = testBindingLayout,
		.bindings = {
			{ .buffer = testGlobalsBuffer },
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
		.triangleCount = numIndices / 3,
		.instanceCount = 1,
	});

	EndPass();

	EndFrame();
}

int main(int argc, char** argv)
{
	GfxInitialize();

	emscripten_set_main_loop(Update, 0, true);

	return 0;
}
