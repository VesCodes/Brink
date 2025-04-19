#include "Brink.h"

using namespace Bk;

struct
{
	uint32 trianglePipeline;
} state;

void Initialize()
{
	state.trianglePipeline = CreatePipeline({
		.name = "Triangle Pipeline",
		.VS = {
			.code = R"(
@vertex fn VsMain(@builtin(vertex_index) position: u32) -> @builtin(position) vec4f
{
    let x = f32(i32(position) - 1);
    let y = f32(i32(position & 1u) * 2 - 1);
    return vec4f(x, y, 0.0, 1.0);
})",
		},
		.PS = {
			.code = R"(
@fragment fn PsMain() -> @location(0) vec4f
{
    return vec4f(0.7, 0.1, 0.3, 1.0);
})",
		},
	});
}

void Update()
{
	BeginPass({
		.name = "Sandbox Pass",
		.clearColor = { 0.2f, 0.2f, 0.3f, 1.0f },
	});

	Draw({
		.pipeline = state.trianglePipeline,
		.vertexCount = 3,
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