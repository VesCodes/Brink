#include "Core/Application.h"
#include "Core/Core.h"
#include "Core/Math.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "Engine/AnimPlayer.h"
#include "Engine/AnimSequence.h"
#include "Engine/Gpu.h"
#include "Engine/Mesh.h"
#include "Engine/Skeleton.h"

#include "GltfLoader.h"

using namespace Bk;

struct
{
	Arena arena;

	uint32 window;
	Vec2f mousePosition;
	Vec2f mouseDelta;
	BitArray keys;
	BitArray lastKeys;
	double lastTime;

	bool initialized;
	uint32 surface;
	uint32 meshPipeline;
	uint32 globalsBuffer;
	uint32 boneTransformsBuffer;
	uint32 globalBindingGroup;

	TSpan<Mesh> meshes;
	TSpan<AnimSequence> animations;
	AnimPlayer animPlayer;
	size_t activeAnimation;

	Vec3f cameraPosition;
	Quat4f cameraOrientation;
	float cameraSpeed;
} state;

bool IsKeyDown(KeyCode keyCode)
{
	size_t keyIdx = static_cast<size_t>(keyCode);
	return GetBit(state.keys, keyIdx);
}

bool IsKeyPressed(KeyCode keyCode)
{
	size_t keyIdx = static_cast<size_t>(keyCode);
	return GetBit(state.keys, keyIdx) && !GetBit(state.lastKeys, keyIdx);
}

void LoadGlb(TSpan<uint8> data)
{
	ArenaScope scratch = GetScratchArena();

	for (Mesh& mesh : state.meshes)
	{
		DestroyBuffer(mesh.positionsBuffer);
		DestroyBuffer(mesh.indicesBuffer);
	}

	state.meshes = {};
	state.animations = {};
	state.animPlayer = {};

	StringBuilder resourceNameBuilder(scratch.arena);

	TSpan<MeshDesc> meshes = {};
	if (LoadGlbMeshes(scratch.arena, data, meshes))
	{
		state.meshes = Push<Mesh>(state.arena, meshes.length);
		for (size_t meshIdx = 0; meshIdx < state.meshes.length; ++meshIdx)
		{
			const MeshDesc& meshDesc = meshes[meshIdx];
			Mesh& mesh = state.meshes[meshIdx];

			mesh.sections = Copy(state.arena, meshDesc.sections);

			Reset(resourceNameBuilder);
			Appendf(resourceNameBuilder, "Mesh%02d_Positions", meshIdx);

			mesh.positionsBuffer = CreateBuffer({
				.name = ToString(resourceNameBuilder, scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = AsBytes(meshDesc.positions),
			});

			Reset(resourceNameBuilder);
			Appendf(resourceNameBuilder, "Mesh%02d_BoneIndices", meshIdx);

			mesh.boneIndicesBuffer = CreateBuffer({
				.name = ToString(resourceNameBuilder, scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = AsBytes(meshDesc.boneIndices),
			});

			Reset(resourceNameBuilder);
			Appendf(resourceNameBuilder, "Mesh%02d_BoneWeights", meshIdx);

			mesh.boneWeightsBuffer = CreateBuffer({
				.name = ToString(resourceNameBuilder, scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = AsBytes(meshDesc.boneWeights),
			});

			Reset(resourceNameBuilder);
			Appendf(resourceNameBuilder, "Mesh%02d_Indices", meshIdx);

			mesh.indicesBuffer = CreateBuffer({
				.name = ToString(resourceNameBuilder, scratch.arena),
				.type = GpuBufferType::Index,
				.data = AsBytes(meshDesc.indices),
			});
		}
	}

	TSpan<Skeleton> skeletons = {};
	if (LoadGlbSkeletons(scratch.arena, data, skeletons) && skeletons.length != 0)
	{
		state.animPlayer.skeleton = skeletons[0];

		state.animPlayer.skeleton.bones = Copy(state.arena, state.animPlayer.skeleton.bones);
		for (Bone& bone : state.animPlayer.skeleton.bones)
		{
			bone.name = Copy(state.arena, bone.name);
		}

		state.animPlayer.skeleton.invBindPose = Copy(state.arena, state.animPlayer.skeleton.invBindPose);
		state.animPlayer.transforms = Push<Mat4f>(state.arena, state.animPlayer.skeleton.bones.length);

		LoadGlbAnimations(state.arena, data, state.animPlayer.skeleton, state.animations);
		if (state.animations.length != 0)
		{
			state.activeAnimation = 0;
			state.animPlayer.animation = state.animations[state.activeAnimation];
			state.animPlayer.currentTime = 0;
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

	state.cameraPosition = Vec3f(0, 1, 4);
	state.cameraOrientation = Quat4f::Identity;
	state.cameraSpeed = 5.0f;

	if (FileHandle fileHandle = OpenFile("Assets/Hiker.glb", FileAccess::Read))
	{
		TSpan<uint8> fileData = Push(scratch.arena, GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			LoadGlb(fileData);
		}

		CloseFile(fileHandle);
	}

	String shaderCode = {};
	if (FileHandle fileHandle = OpenFile("Assets/Basic.wgsl", FileAccess::Read))
	{
		TSpan<uint8> fileData = Push(scratch.arena, GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			shaderCode = String(reinterpret_cast<char*>(fileData.data), fileData.length);
		}

		CloseFile(fileHandle);
	}

	uint32 globalBindingLayout = CreateBindingLayout({
		.name = "Global Binding Layout",
		.bindings = {
			{ .type = GpuBindingType::UniformBuffer, .stage = GpuBindingStage::Vertex },
			{ .type = GpuBindingType::ReadOnlyStorageBuffer, .stage = GpuBindingStage::Vertex },
		},
	});

	state.meshPipeline = CreateRenderPipeline({
		.name = "Mesh Pipeline",
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
			globalBindingLayout,
		},
	});

	state.globalsBuffer = CreateBuffer({
		.name = "Globals",
		.type = GpuBufferType::Uniform,
		.access = GpuBufferAccess::GpuOnly,
		.size = sizeof(Mat4f),
	});

	state.boneTransformsBuffer = CreateBuffer({
		.name = "Bone Transforms",
		.type = GpuBufferType::Storage,
		.access = GpuBufferAccess::GpuOnly,
		.size = sizeof(Mat4f) * 512,
	});

	state.globalBindingGroup = CreateBindingGroup({
		.name = "Global Binding Group",
		.bindingLayout = globalBindingLayout,
		.bindings = {
			{ .buffer = state.globalsBuffer },
			{ .buffer = state.boneTransformsBuffer },
		},
	});
}

bool OnAppUpdate()
{
	ArenaScope scratch = GetScratchArena();

	bool result = state.window != 0;

	if (!BeginFrame())
	{
		return result;
	}

	if (!state.initialized)
	{
		Initialize();
		state.initialized = true;
		state.lastTime = GetTimeSec();
	}

	double currentTime = GetTimeSec();
	float deltaTime = static_cast<float>(currentTime - state.lastTime);

	if (IsKeyDown(KeyCode::RightMouseButton))
	{
		Vec3f cameraMove = Vec3f::Zero;

		if (IsKeyDown(KeyCode::W))
		{
			cameraMove.z -= 1;
		}

		if (IsKeyDown(KeyCode::S))
		{
			cameraMove.z += 1;
		}

		if (IsKeyDown(KeyCode::A))
		{
			cameraMove.x -= 1;
		}

		if (IsKeyDown(KeyCode::D))
		{
			cameraMove.x += 1;
		}

		if (IsKeyDown(KeyCode::Q))
		{
			state.cameraPosition.y -= state.cameraSpeed * deltaTime;
		}

		if (IsKeyDown(KeyCode::E))
		{
			state.cameraPosition.y += state.cameraSpeed * deltaTime;
		}

		if (state.mouseDelta.x != 0 || state.mouseDelta.y != 0)
		{
			if (state.mouseDelta.x != 0)
			{
				Vec3f worldUp = Vec3f(0, 1, 0);
				Vec3f cameraUp = RotateVector(state.cameraOrientation, worldUp);

				float yawAngle = -state.mouseDelta.x * 0.005f * (Dot(worldUp, cameraUp) < 0 ? -1.0f : 1.0f);
				Quat4f yawRotation = AxisAngleRotation(worldUp, yawAngle);

				state.cameraOrientation = yawRotation * state.cameraOrientation;
			}

			if (state.mouseDelta.y != 0)
			{
				float pitchAngle = -state.mouseDelta.y * 0.005f;
				Quat4f pitchRotation = AxisAngleRotation(Vec3f(1, 0, 0), pitchAngle);

				state.cameraOrientation = state.cameraOrientation * pitchRotation;
			}

			state.cameraOrientation = Normalize(state.cameraOrientation);
		}

		if (cameraMove.x != 0 || cameraMove.z != 0)
		{
			cameraMove = Normalize(cameraMove);
			state.cameraPosition += RotateVector(state.cameraOrientation, cameraMove) * state.cameraSpeed * deltaTime;
		}
	}

	if (IsKeyPressed(KeyCode::X))
	{
		state.activeAnimation = (state.activeAnimation + 1) % state.animations.length;
		state.animPlayer.animation = state.animations[state.activeAnimation];
		state.animPlayer.currentTime = 0;
	}

	Mat4f proj = PerspectiveMatrix(30.0f * (3.14f / 180.0f), 16.0f / 9.0f, 0.01f, 100.0f);
	Mat4f view = RotationMatrix(Conjugate(state.cameraOrientation)) * TranslationMatrix(-state.cameraPosition);
	Mat4f model = Mat4f::Identity;

	Mat4f mvp = proj * view * model;

	WriteBuffer(state.globalsBuffer, AsBytes(&mvp, 1));

	if (state.animPlayer.transforms.length != 0)
	{
		state.animPlayer.Update(deltaTime);
		WriteBuffer(state.boneTransformsBuffer, AsBytes(state.animPlayer.transforms));
	}

	BeginRenderPass({
		.name = "Sandbox Pass",
		.surface = state.surface,
		.clearColor = { 0.12f, 0.12f, 0.14f, 1.0f },
	});

	for (const Mesh& mesh : state.meshes)
	{
		for (const MeshSection& section : mesh.sections)
		{
			Draw({
				.pipeline = state.meshPipeline,
				.bindingGroups = {
					state.globalBindingGroup,
				},
				.vertexBuffers = {
					mesh.positionsBuffer,
					mesh.boneIndicesBuffer,
					mesh.boneWeightsBuffer,
				},
				.indexBuffer = mesh.indicesBuffer,
				.vertexOffset = static_cast<uint32>(section.vertexOffset),
				.indexOffset = static_cast<uint32>(section.indexOffset),
				.triangleCount = static_cast<uint32>(section.triangleCount),
				.instanceCount = 1,
			});
		}
	}

	EndRenderPass();

	EndFrame();
	PresentSurface(state.surface);

	state.mouseDelta = Vec2f::Zero;
	state.lastTime = currentTime;
	Copy(state.lastKeys, state.keys);

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
				SetBit(state.keys, static_cast<size_t>(appEvent.keyCode));
			}
			else
			{
				ClearBit(state.keys, static_cast<size_t>(appEvent.keyCode));
			}

			break;
		}

		case AppEventType::MouseMove:
		{
			Vec2f mousePosition(appEvent.mouseX, appEvent.mouseY);
			state.mouseDelta += (mousePosition - state.mousePosition);
			state.mousePosition = mousePosition;

			break;
		}

		case AppEventType::MouseWheel:
		{
			state.cameraSpeed = Clamp(state.cameraSpeed + appEvent.wheelDelta, 1.0f, 42.0f);

			break;
		}

		case AppEventType::DropFile:
		{
			if (FileHandle fileHandle = OpenFile(appEvent.dropFilePath, FileAccess::Read))
			{
				ArenaScope scratch = GetScratchArena();

				TSpan<uint8> fileData = Push(scratch.arena, GetFileSize(fileHandle));
				if (ReadFile(fileHandle, fileData) == fileData.length)
				{
					LoadGlb(fileData);
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

	Allocate(state.arena, state.keys, static_cast<size_t>(KeyCode::Count));
	Allocate(state.arena, state.lastKeys, static_cast<size_t>(KeyCode::Count));

	ConfigureApp({
		.updateCallback = OnAppUpdate,
		.eventCallback = OnAppEvent,
	});

	GpuInitialize();

	return 0;
}
