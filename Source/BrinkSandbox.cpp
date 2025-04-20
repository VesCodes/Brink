#include "Brink.h"

#include <math.h>

using namespace Bk;

struct
{
	Arena arena;
	uint32 testPipeline;
	uint32 testVertexBuffer;
	uint32 testIndexBuffer;
} state;

const char* testShader = R"(
@vertex fn VsMain(@location(0) position: vec2f) -> @builtin(position) vec4f
{
    return vec4f(position, 0.0, 1.0);
}

@fragment fn PsMain() -> @location(0) vec4f
{
    return vec4f(0.2, 0.7, 0.3, 1.0);
}
)";

uint32 subdivisions = 64;
uint32 numVertices = (subdivisions + 1) * 2;
uint32 numIndices = subdivisions * 3;

void Initialize()
{
	state.arena.Initialize(BK_MEGABYTES(2));

	state.testPipeline = CreatePipeline({
		.name = "Test Pipeline",
		.VS = {
			.code = testShader,
			.buffers = {
				{
					.stride = 8,
					.attributes = {
						{ .offset = 0, .format = GpuVertexFormat::Float32x2 },
					},
				},
			},
		},
		.PS = { .code = testShader },
	});

	float* vertices = state.arena.Push<float>(numVertices);
	uint16* indices = state.arena.Push<uint16>(numIndices);

	float radius = 0.75f;

	vertices[0] = vertices[1] = 0;
	for(int i = 0; i < subdivisions; ++i)
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
		.type = GpuBufferType::Vertex,
		.data = Span((uint8*)vertices, numVertices * sizeof(vertices[0])),
	});

	state.testIndexBuffer = CreateBuffer({
		.name = "Test Index Buffer",
		.type = GpuBufferType::Index,
		.data = Span((uint8*)indices, numIndices * sizeof(indices[0])),
	});
}

void Update()
{
	BeginPass({
		.name = "Sandbox Pass",
		.clearColor = { 0.2f, 0.2f, 0.3f, 1.0f },
	});

	Draw({
		.pipeline = state.testPipeline,
		.vertexBuffer = state.testVertexBuffer,
		.indexBuffer = state.testIndexBuffer,
		.triangleCount = numIndices / 3,
		.instanceCount = 1,
	});

	EndPass();
}

AppDesc Bk::Main(int32 argc, char** argv)
{
	return {
		.initialize = Initialize,
		.update = Update,
	};
}