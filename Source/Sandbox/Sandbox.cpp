#include "Core/Application.h"
#include "Core/Core.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "Renderer/Gpu.h"

#include "GltfLoader.h"

#define HANDMADE_MATH_USE_DEGREES
#include <HandmadeMath.h>

using namespace Bk;

struct MeshProxy
{
	TSpan<MeshSection> sections;
	uint32 vertexBuffer;
	uint32 boneIndexBuffer;
	uint32 boneWeightBuffer;
	uint32 indexBuffer;
};

struct
{
	Arena arena;

	uint32 window;
	HMM_Vec2 mousePosition;
	HMM_Vec2 mouseDelta;
	uint32* keys;

	bool initialized;
	uint32 surface;
	uint32 testPipeline;
	uint32 testUniformBuffer;
	uint32 testBindingGroup;

	TSpan<MeshProxy> meshProxies;

	HMM_Vec3 cameraPosition;
	HMM_Quat cameraOrientation;
	float cameraSpeed;
} state;

bool IsKeyDown(KeyCode keyCode)
{
	return BitsetIsSet(state.keys, (size_t)keyCode);
}

void LoadGlb(uint8* data, size_t length)
{
	ArenaScope scratch = GetScratchArena();

	for (MeshProxy& meshProxy : state.meshProxies)
	{
		DestroyBuffer(meshProxy.vertexBuffer);
		DestroyBuffer(meshProxy.indexBuffer);
	}

	state.meshProxies = {};

	StringBuilder resourceNameBuilder(scratch.arena);

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

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_VertexBuffer", meshIdx);

			meshProxy.vertexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = mesh.positionBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_BoneIndexBuffer", meshIdx);

			meshProxy.boneIndexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = mesh.boneIndexBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_BoneWeightBuffer", meshIdx);

			meshProxy.boneWeightBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = mesh.boneWeightBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_IndexBuffer", meshIdx);

			meshProxy.indexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Index,
				.data = mesh.indexBuffer,
			});
		}
	}
}

void Initialize()
{
	ArenaScope scratch = GetScratchArena();

	uint32 surfaceWidth, surfaceHeight;
	if (GetWindowSurfaceSize(state.window, surfaceWidth, surfaceHeight))
	{
		state.surface = CreateSurface(
			GetWindowSurfaceTarget(state.window),
			{
				.width = surfaceWidth,
				.height = surfaceHeight,
			});
	}

	state.cameraPosition = HMM_V3(0, 1, 4);
	state.cameraOrientation = HMM_Q(0, 0, 0, 1);
	state.cameraSpeed = 0.025f;

	if (FileHandle fileHandle = OpenFile("Assets/Hiker.glb", FileAccess::Read))
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
				{
					.stride = 4,
					.attributes = {
						{ .offset = 0, .format = GpuVertexFormat::Uint8x4 },
					},
				},
				{
					.stride = 16,
					.attributes = {
						{ .offset = 0, .format = GpuVertexFormat::Float32x4 },
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

bool OnAppUpdate()
{
	bool result = state.window != 0;

	if (!BeginFrame())
	{
		return result;
	}

	if (!state.initialized)
	{
		Initialize();
		state.initialized = true;
	}

	if (IsKeyDown(KeyCode::RightMouseButton))
	{
		HMM_Vec3 cameraMove = HMM_V3(0, 0, 0);

		if (IsKeyDown(KeyCode::W))
		{
			cameraMove.Z -= 1;
		}

		if (IsKeyDown(KeyCode::S))
		{
			cameraMove.Z += 1;
		}

		if (IsKeyDown(KeyCode::A))
		{
			cameraMove.X -= 1;
		}

		if (IsKeyDown(KeyCode::D))
		{
			cameraMove.X += 1;
		}

		if (IsKeyDown(KeyCode::Q))
		{
			state.cameraPosition.Y -= state.cameraSpeed;
		}

		if (IsKeyDown(KeyCode::E))
		{
			state.cameraPosition.Y += state.cameraSpeed;
		}

		if (state.mouseDelta.X != 0 || state.mouseDelta.Y != 0)
		{
			if (state.mouseDelta.X != 0)
			{
				HMM_Vec3 worldUp = HMM_V3(0, 1, 0);
				HMM_Vec3 cameraUp = HMM_RotateV3Q(worldUp, state.cameraOrientation);

				float yawAngle = -state.mouseDelta.X * 0.25f * (HMM_DotV3(worldUp, cameraUp) < 0 ? -1.0f : 1.0f);
				HMM_Quat yawRotation = HMM_QFromAxisAngle_RH(HMM_V3(0, 1, 0), yawAngle);

				state.cameraOrientation = yawRotation * state.cameraOrientation;
			}

			if (state.mouseDelta.Y != 0)
			{
				float pitchAngle = -state.mouseDelta.Y * 0.25f;
				HMM_Quat pitchRotation = HMM_QFromAxisAngle_RH(HMM_V3(1, 0, 0), pitchAngle);

				state.cameraOrientation = state.cameraOrientation * pitchRotation;
			}

			state.cameraOrientation = HMM_NormQ(state.cameraOrientation);
		}

		if (cameraMove.X != 0 || cameraMove.Z != 0)
		{
			cameraMove = HMM_NormV3(cameraMove);
			state.cameraPosition += HMM_RotateV3Q(cameraMove, state.cameraOrientation) * state.cameraSpeed;
		}
	}

	HMM_Mat4 proj = HMM_Perspective_RH_ZO(60, 16.0f / 9.0f, 0.01f, 100);
	HMM_Mat4 view = HMM_QToM4(HMM_InvQ(state.cameraOrientation)) * HMM_Translate(state.cameraPosition * -1.0f);
	HMM_Mat4 model = HMM_M4D(1);

	HMM_Mat4 mvp = proj * view * model;

	WriteBuffer(state.testUniformBuffer, TSpan((uint8*)mvp.Elements, sizeof(mvp)));

	BeginPass({
		.name = "Sandbox Pass",
		.surface = state.surface,
		.clearColor = { 0.12f, 0.12f, 0.14f, 1.0f },
	});

	for (const MeshProxy& meshProxy : state.meshProxies)
	{
		for (const MeshSection& meshSection : meshProxy.sections)
		{
			Draw({
				.pipeline = state.testPipeline,
				.vertexBuffers = {
					meshProxy.vertexBuffer,
					meshProxy.boneIndexBuffer,
					meshProxy.boneWeightBuffer,
				},
				.indexBuffer = meshProxy.indexBuffer,
				.bindingGroups = {
					state.testBindingGroup,
				},
				.vertexOffset = static_cast<uint32>(meshSection.vertexOffset),
				.indexOffset = static_cast<uint32>(meshSection.indexOffset),
				.triangleCount = static_cast<uint32>(meshSection.triangleCount),
				.instanceCount = 1,
			});
		}
	}

	EndPass();

	EndFrame();

	PresentSurface(state.surface);

	state.mouseDelta = HMM_V2(0, 0);

	return result;
}

bool OnAppEvent(const AppEvent& appEvent)
{
	bool result = false;

	switch (appEvent.type)
	{
		case AppEventType::Key:
		{
			if (appEvent.keyPressed)
			{
				BitsetSet(state.keys, size_t(appEvent.keyCode));
			}
			else
			{
				BitsetUnset(state.keys, size_t(appEvent.keyCode));
			}

			break;
		}

		case AppEventType::MouseMove:
		{
			HMM_Vec2 mousePosition = HMM_V2(appEvent.mouseX, appEvent.mouseY);
			state.mouseDelta += (mousePosition - state.mousePosition);
			state.mousePosition = mousePosition;

			break;
		}

		case AppEventType::MouseWheel:
		{
			state.cameraSpeed = Clamp(state.cameraSpeed + appEvent.wheelDelta * 0.0025f, 0.0001f, 2.0f);

			break;
		}

		case AppEventType::DropFile:
		{
			if (FileHandle fileHandle = OpenFile(appEvent.dropFilePath, FileAccess::Read))
			{
				ArenaScope scratch = GetScratchArena();

				TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
				if (ReadFile(fileHandle, fileData) == fileData.length)
				{
					LoadGlb(fileData.data, fileData.length);
				}

				CloseFile(fileHandle);
				result = true;
			}

			break;
		}

		case AppEventType::WindowResize:
		{
			if (appEvent.target == state.window)
			{
				uint32 surfaceWidth, surfaceHeight;
				if (GetWindowSurfaceSize(state.window, surfaceWidth, surfaceHeight))
				{
					ConfigureSurface(state.surface, { .width = surfaceWidth, .height = surfaceHeight });
					result = true;
				}
			}

			break;
		}

		case AppEventType::WindowClose:
		{
			if (appEvent.target == state.window)
			{
				state.window = 0;
				result = true;
			}

			break;
		}

		default: break;
	}

	return result;
}

int32 AppMain(int32 argc, char** argv)
{
	state.window = CreateWindow({ .title = "#canvas", .width = 1280, .height = 720 });
	if (!state.window)
	{
		return 1;
	}

	state.keys = state.arena.PushZeroed<uint32>((size_t(KeyCode::Count) + 31) / 32);

	ConfigureApp({
		.updateCallback = OnAppUpdate,
		.eventCallback = OnAppEvent,
	});

	GpuInitialize();

	return 0;
}
