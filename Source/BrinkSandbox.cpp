#include "Brink.h"

using namespace Bk;

struct
{
	uint32 testPipeline;
	uint32 testVertexBuffer;
} state;

// clang-format off
float testVertices[] = {
	0.0f,  0.0f,  0.75f,  	  0.0f,		  0.735589f,  0.146318f,
	0.0f,  0.0f,  0.735589f,  0.146318f,  0.692910f,  0.287012f,
	0.0f,  0.0f,  0.692910f,  0.287012f,  0.623603f,  0.416677f,
	0.0f,  0.0f,  0.623603f,  0.416677f,  0.530330f,  0.530330f,
	0.0f,  0.0f,  0.530330f,  0.530330f,  0.416677f,  0.623603f,
	0.0f,  0.0f,  0.416677f,  0.623603f,  0.287012f,  0.692910f,
	0.0f,  0.0f,  0.287012f,  0.692910f,  0.146318f,  0.735589f,
	0.0f,  0.0f,  0.146318f,  0.735589f,  0.000000f,  0.75f,
	0.0f,  0.0f,  0.000000f,  0.75f, 	 -0.146318f,  0.735589f,
	0.0f,  0.0f, -0.146318f,  0.735589f, -0.287012f,  0.692910f,
	0.0f,  0.0f, -0.287012f,  0.692910f, -0.416677f,  0.623603f,
	0.0f,  0.0f, -0.416677f,  0.623603f, -0.530330f,  0.530330f,
	0.0f,  0.0f, -0.530330f,  0.530330f, -0.623603f,  0.416677f,
	0.0f,  0.0f, -0.623603f,  0.416677f, -0.692910f,  0.287012f,
	0.0f,  0.0f, -0.692910f,  0.287012f, -0.735589f,  0.146318f,
	0.0f,  0.0f, -0.735589f,  0.146318f, -0.75f,       0.000000f,
	0.0f,  0.0f, -0.75f,      0.000000f, -0.735589f, -0.146318f,
	0.0f,  0.0f, -0.735589f, -0.146318f, -0.692910f, -0.287012f,
	0.0f,  0.0f, -0.692910f, -0.287012f, -0.623603f, -0.416677f,
	0.0f,  0.0f, -0.623603f, -0.416677f, -0.530330f, -0.530330f,
	0.0f,  0.0f, -0.530330f, -0.530330f, -0.416677f, -0.623603f,
	0.0f,  0.0f, -0.416677f, -0.623603f, -0.287012f, -0.692910f,
	0.0f,  0.0f, -0.287012f, -0.692910f, -0.146318f, -0.735589f,
	0.0f,  0.0f, -0.146318f, -0.735589f,  0.000000f, -0.75f,
	0.0f,  0.0f,  0.000000f, -0.75f,  	  0.146318f, -0.735589f,
	0.0f,  0.0f,  0.146318f, -0.735589f,  0.287012f, -0.692910f,
	0.0f,  0.0f,  0.287012f, -0.692910f,  0.416677f, -0.623603f,
	0.0f,  0.0f,  0.416677f, -0.623603f,  0.530330f, -0.530330f,
	0.0f,  0.0f,  0.530330f, -0.530330f,  0.623603f, -0.416677f,
	0.0f,  0.0f,  0.623603f, -0.416677f,  0.692910f, -0.287012f,
	0.0f,  0.0f,  0.692910f, -0.287012f,  0.735589f, -0.146318f,
	0.0f,  0.0f,  0.735589f, -0.146318f,  0.75f,  	  0.0f,
};
// clang-format on

void Initialize()
{
	state.testPipeline = CreatePipeline({
		.name = "Test Pipeline",
		.VS = {
			.code = R"(
@vertex fn VsMain(@location(0) position: vec2f) -> @builtin(position) vec4f
{
    return vec4f(position, 0.0, 1.0);
})",
			.buffers = {
				{
					.stride = 8,
					.attributes = {
						{ .offset = 0, .format = GpuVertexFormat::Float32x2 },
					},
				},
			},
		},
		.PS = {
			.code = R"(
@fragment fn PsMain() -> @location(0) vec4f
{
    return vec4f(0.7, 0.1, 0.3, 1.0);
})",
		},
	});

	state.testVertexBuffer = CreateBuffer({
		.name = "Test Vertex Buffer",
		.type = GpuBufferType::Vertex,
		.data = Span((uint8*)testVertices, sizeof(testVertices)),
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
		.vertexCount = BK_ARRAY_COUNT(testVertices) / 2,
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