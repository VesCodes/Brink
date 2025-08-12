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

struct MeshProxy
{
	TSpan<MeshSection> sections;
	uint32 vertexBuffer;
	uint32 indexBuffer;
};

struct
{
	bool initialized;

	Arena arena;

	uint32 testPipeline;
	uint32 testUniformBuffer;
	uint32 testBindingGroup;

	TSpan<MeshProxy> meshProxies;
} state;

const char* testShader = R"(
@group(0) @binding(0) var<uniform> mvp: mat4x4f;

struct VsInput
{
	@location(0) position: vec3f,
}

struct VsOutput {
	@builtin(position) position: vec4f,
	@location(0) worldPosition: vec3f,
}

@vertex fn VsMain(input: VsInput) -> VsOutput {
	var output: VsOutput;
	output.position = mvp * vec4(input.position, 1);
	output.worldPosition = input.position;
	return output;
}

struct PsInput
{
	@location(0) worldPosition: vec3f,
}

@fragment fn PsMain(input: PsInput) -> @location(0) vec4f {
	let dx = dpdx(input.worldPosition);
	let dy = dpdy(input.worldPosition);
	let normal = normalize(cross(dx, dy));
	return vec4((normal + 1.0) * 0.5, 1.0);
}
)";

extern "C" EMSCRIPTEN_KEEPALIVE void LoadGlb(uint8* data, size_t length)
{
	ArenaScope scratch = GetScratchArena();

	for (MeshProxy& meshProxy : state.meshProxies)
	{
		DestroyBuffer(meshProxy.vertexBuffer);
		DestroyBuffer(meshProxy.indexBuffer);
	}

	state.meshProxies = {};

	TSpan<Mesh> meshes;
	if (LoadGlbMeshes(scratch.arena, TSpan(data, length), meshes))
	{
		state.meshProxies = state.arena.Push<MeshProxy>(meshes.length);
		for (size_t meshIdx = 0; meshIdx < state.meshProxies.length; ++meshIdx)
		{
			const Mesh& mesh = meshes[meshIdx];
			MeshProxy& meshProxy = state.meshProxies[meshIdx];

			meshProxy.sections = state.arena.Push<MeshSection>(mesh.sections.length);
			MemoryCopy(meshProxy.sections.data, mesh.sections.data, mesh.sections.length * sizeof(MeshSection));

			meshProxy.vertexBuffer = CreateBuffer({
				.type = GpuBufferType::Vertex,
				.data = mesh.positionBuffer,
			});

			meshProxy.indexBuffer = CreateBuffer({
				.type = GpuBufferType::Index,
				.data = mesh.indexBuffer,
			});
		}
	}
}

void Initialize()
{
	ArenaScope scratch = GetScratchArena();

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

	FileHandle fileHandle = OpenFile("Assets/Knight.glb", FileAccess::Read);
	if (fileHandle)
	{
		TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			LoadGlb(fileData.data, fileData.length);
		}

		CloseFile(fileHandle);
	}
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
	HMM_Mat4 view = HMM_LookAt_RH(HMM_V3(0, 2, 4), HMM_V3(0, 1, 0), HMM_V3(0, 1, 0));
	HMM_Mat4 model = HMM_Translate(HMM_V3(0, HMM_SinF(t * 12 * HMM_PI32) * 0.5f, 0)) * HMM_Rotate_RH(t * 64, HMM_V3(0, 1, 0));

	HMM_Mat4 mvp = proj * view * model;

	WriteBuffer(state.testUniformBuffer, TSpan((uint8*)mvp.Elements, sizeof(mvp)));

	BeginPass({
		.name = "Sandbox Pass",
		.clearColor = { 0.2f, 0.2f, 0.3f, 1.0f },
	});

	for (const MeshProxy& meshProxy : state.meshProxies)
	{
		for (const MeshSection& meshSection : meshProxy.sections)
		{
			Draw({
				.pipeline = state.testPipeline,
				.vertexBuffer = meshProxy.vertexBuffer,
				.indexBuffer = meshProxy.indexBuffer,
				.bindingGroups = {
					state.testBindingGroup,
				},
				.vertexOffset = meshSection.vertexOffset,
				.indexOffset = meshSection.indexOffset,
				.triangleCount = meshSection.triangleCount,
				.instanceCount = 1,
			});
		}
	}

	EndPass();

	EndFrame();
}

int main(int argc, char** argv)
{
	GpuInitialize();

	emscripten_set_main_loop(Update, 0, true);

	return 0;
}
