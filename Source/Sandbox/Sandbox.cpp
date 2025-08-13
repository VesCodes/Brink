#include "Core/Core.h"
#include "Core/Gpu.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "GltfLoader.h"

#define HANDMADE_MATH_USE_DEGREES
#include <HandmadeMath.h>

#include <emscripten/dom_pk_codes.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

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

	HMM_Vec2 mousePosition;
	uint32* keys;

	HMM_Vec3 cameraPosition;
	HMM_Quat cameraOrientation;
} state;

bool IsKeyDown(int32 keyCode)
{
	return BitsetIsSet(state.keys, keyCode);
}

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

	state.keys = state.arena.PushZeroed<uint32>((DOM_PK_MEDIA_SELECT + 31) / 32);

	state.cameraPosition = HMM_V3(0, 1, 4);
	state.cameraOrientation = HMM_Q(0, 0, 0, 1);

	if (FileHandle fileHandle = OpenFile("Assets/Knight.glb", FileAccess::Read))
	{
		TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			LoadGlb(fileData.data, fileData.length);
		}

		CloseFile(fileHandle);
	}

	String shaderCode = {};
	if (FileHandle fileHandle = OpenFile("Assets/Basic.wgsl", FileAccess::Read))
	{
		TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			shaderCode = String((char*)fileData.data, fileData.length);
		}

		CloseFile(fileHandle);
	}

	uint32 testBindingLayout = CreateBindingLayout({
		.name = "Test Binding Layout",
		.bindings = {
			{ .type = GpuBindingType::UniformBuffer, .stage = GpuBindingStage::Vertex },
		},
	});

	state.testPipeline = CreatePipeline({
		.name = "Test Pipeline",
		.vertexShader = {
			.code = shaderCode,
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
			.code = shaderCode,
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

	HMM_Vec3 cameraMove = HMM_V3(0, 0, 0);
	float cameraSpeed = 0.05f;

	if (IsKeyDown(DOM_PK_Q))
	{
		state.cameraPosition.Y -= cameraSpeed;
	}

	if (IsKeyDown(DOM_PK_E))
	{
		state.cameraPosition.Y += cameraSpeed;
	}

	if (IsKeyDown(DOM_PK_W))
	{
		cameraMove.Z -= 1;
	}

	if (IsKeyDown(DOM_PK_S))
	{
		cameraMove.Z += 1;
	}

	if (IsKeyDown(DOM_PK_A))
	{
		cameraMove.X -= 1;
	}

	if (IsKeyDown(DOM_PK_D))
	{
		cameraMove.X += 1;
	}

	if (cameraMove.X != 0 || cameraMove.Z != 0)
	{
		cameraMove = HMM_NormV3(cameraMove);
		state.cameraPosition += HMM_RotateV3Q(cameraMove, state.cameraOrientation) * cameraSpeed;
	}

	HMM_Mat4 proj = HMM_Perspective_RH_ZO(60, 16.0f / 9.0f, 0.01f, 100);
	HMM_Mat4 view = HMM_QToM4(HMM_InvQ(state.cameraOrientation)) * HMM_Translate(state.cameraPosition * -1.0f);
	HMM_Mat4 model = HMM_M4D(1);

	HMM_Mat4 mvp = proj * view * model;

	WriteBuffer(state.testUniformBuffer, TSpan((uint8*)mvp.Elements, sizeof(mvp)));

	BeginPass({
		.name = "Sandbox Pass",
		.clearColor = { 0.12f, 0.12f, 0.14f, 1.0f },
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

bool OnKeyEvent(int32 eventType, const EmscriptenKeyboardEvent* event, void* userData)
{
	if (state.keys)
	{
		if (eventType == EMSCRIPTEN_EVENT_KEYDOWN)
		{
			int32 keyCode = emscripten_compute_dom_pk_code(event->code);
			BitsetSet(state.keys, keyCode);
		}
		else if (eventType == EMSCRIPTEN_EVENT_KEYUP)
		{
			int32 keyCode = emscripten_compute_dom_pk_code(event->code);
			BitsetUnset(state.keys, keyCode);
		}
	}

	return false;
}

bool OnMouseEvent(int32 eventType, const EmscriptenMouseEvent* event, void* userData)
{
	bool handled = false;

	if (eventType == EMSCRIPTEN_EVENT_MOUSEMOVE)
	{
		HMM_Vec2 mousePosition = HMM_V2(static_cast<float>(event->screenX), static_cast<float>(event->screenY));

		if ((event->buttons & (1 << 0)) != 0)
		{
			HMM_Vec2 mouseDelta = mousePosition - state.mousePosition;

			HMM_Vec3 cameraRight = HMM_RotateV3Q(HMM_V3(1, 0, 0), state.cameraOrientation);

			HMM_Quat yawRotation = HMM_QFromAxisAngle_RH(HMM_V3(0, 1, 0), -mouseDelta.X * 0.5f);
			HMM_Quat pitchRotation = HMM_QFromAxisAngle_RH(cameraRight, -mouseDelta.Y * 0.5f);

			state.cameraOrientation = (pitchRotation * state.cameraOrientation * yawRotation);

			handled = true;
		}

		state.mousePosition = mousePosition;
	}

	return handled;
}

int main(int argc, char** argv)
{
	GpuInitialize();

	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnKeyEvent);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnKeyEvent);
	emscripten_set_mousemove_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnMouseEvent);
	emscripten_set_mousedown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnMouseEvent);
	emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, nullptr, true, OnMouseEvent);

	emscripten_set_main_loop(Update, 0, true);

	return 0;
}
