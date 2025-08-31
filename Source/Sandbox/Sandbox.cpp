#include "Core/Application.h"
#include "Core/Core.h"
#include "Core/Math.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "Engine/AnimPlayer.h"
#include "Engine/AnimSequence.h"
#include "Engine/Mesh.h"
#include "Engine/Skeleton.h"

#include "Renderer/Gpu.h"

#include "GltfLoader.h"

using namespace Bk;

struct
{
	Arena arena;

	uint32 window;
	Vec2f mousePosition;
	Vec2f mouseDelta;
	uint32* keys;
	double lastTime;

	bool initialized;
	uint32 surface;
	uint32 meshPipeline;
	uint32 globalsBuffer;
	uint32 jointTransformsBuffer;
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
	return BitsetIsSet(state.keys, static_cast<size_t>(keyCode));
}

void LoadGlb(TSpan<uint8> data)
{
	ArenaScope scratch = GetScratchArena();

	for (Mesh& mesh : state.meshes)
	{
		DestroyBuffer(mesh.vertexBuffer);
		DestroyBuffer(mesh.indexBuffer);
	}

	state.meshes = {};
	state.animations = {};
	state.animPlayer = {};

	StringBuilder resourceNameBuilder(scratch.arena);

	TSpan<MeshDesc> meshes = {};
	if (LoadGlbMeshes(scratch.arena, data, meshes))
	{
		state.meshes = state.arena.Push<Mesh>(meshes.length);
		for (size_t meshIdx = 0; meshIdx < state.meshes.length; ++meshIdx)
		{
			const MeshDesc& meshDesc = meshes[meshIdx];
			Mesh& mesh = state.meshes[meshIdx];

			mesh.sections = state.arena.Copy(meshDesc.sections);

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_VertexBuffer", meshIdx);

			mesh.vertexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = meshDesc.positionBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_JointIndexBuffer", meshIdx);

			mesh.jointIndexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = meshDesc.jointIndexBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_JointWeightBuffer", meshIdx);

			mesh.jointWeightBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = meshDesc.jointWeightBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_IndexBuffer", meshIdx);

			mesh.indexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Index,
				.data = meshDesc.indexBuffer,
			});
		}
	}

	TSpan<Skeleton> skeletons = {};
	if (LoadGlbSkeletons(scratch.arena, data, skeletons) && skeletons.length > 0)
	{
		state.animPlayer.skeleton = skeletons[0];

		state.animPlayer.skeleton.bones = state.arena.Copy(state.animPlayer.skeleton.bones);
		for (Bone& bone : state.animPlayer.skeleton.bones)
		{
			bone.name = state.arena.Copy(bone.name);
		}

		state.animPlayer.skeleton.invBindPose = state.arena.Copy(state.animPlayer.skeleton.invBindPose);
		state.animPlayer.transforms = state.arena.Push<Mat4f>(state.animPlayer.skeleton.bones.length);

		LoadGlbAnimations(state.arena, data, state.animPlayer.skeleton, state.animations);
		if (state.animations.length > 0)
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
		TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
		if (ReadFile(fileHandle, fileData) == fileData.length)
		{
			LoadGlb(fileData);
		}

		CloseFile(fileHandle);
	}

	String shaderCode = {};
	if (FileHandle fileHandle = OpenFile("Assets/Basic.wgsl", FileAccess::Read))
	{
		TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
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

	state.meshPipeline = CreatePipeline({
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
		.name = "Globals Buffer",
		.type = GpuBufferType::Uniform,
		.access = GpuBufferAccess::GpuOnly,
		.size = sizeof(Mat4f),
	});

	state.jointTransformsBuffer = CreateBuffer({
		.name = "Joint Transforms Buffer",
		.type = GpuBufferType::Storage,
		.access = GpuBufferAccess::GpuOnly,
		.size = sizeof(Mat4f) * 512,
	});

	state.globalBindingGroup = CreateBindingGroup({
		.name = "Global Binding Group",
		.bindingLayout = globalBindingLayout,
		.bindings = {
			{ .buffer = state.globalsBuffer },
			{ .buffer = state.jointTransformsBuffer },
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

	Mat4f proj = PerspectiveMatrix(30.0f * (3.14f / 180.0f), 16.0f / 9.0f, 0.01f, 100.0f);
	Mat4f view = RotationMatrix(Conjugate(state.cameraOrientation)) * TranslationMatrix(-state.cameraPosition);
	Mat4f model = Mat4f::Identity;

	Mat4f mvp = proj * view * model;

	WriteBuffer(state.globalsBuffer, TSpan(reinterpret_cast<uint8*>(mvp.elements), sizeof(Mat4f)));

	if (state.animPlayer.transforms.length != 0)
	{
		state.animPlayer.Update(deltaTime);
		WriteBuffer(state.jointTransformsBuffer, TSpan(reinterpret_cast<uint8*>(state.animPlayer.transforms.data), sizeof(Mat4f) * state.animPlayer.transforms.length));
	}

	BeginPass({
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
				.vertexBuffers = {
					mesh.vertexBuffer,
					mesh.jointIndexBuffer,
					mesh.jointWeightBuffer,
				},
				.indexBuffer = mesh.indexBuffer,
				.bindingGroups = {
					state.globalBindingGroup,
				},
				.vertexOffset = static_cast<uint32>(section.vertexOffset),
				.indexOffset = static_cast<uint32>(section.indexOffset),
				.triangleCount = static_cast<uint32>(section.triangleCount),
				.instanceCount = 1,
			});
		}
	}

	EndPass();

	EndFrame();
	PresentSurface(state.surface);

	state.mouseDelta = Vec2f::Zero;
	state.lastTime = currentTime;

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
				BitsetSet(state.keys, static_cast<size_t>(appEvent.keyCode));
			}
			else
			{
				BitsetUnset(state.keys, static_cast<size_t>(appEvent.keyCode));

				if (appEvent.keyCode == KeyCode::X)
				{
					state.activeAnimation = (state.activeAnimation + 1) % state.animations.length;
					state.animPlayer.animation = state.animations[state.activeAnimation];
					state.animPlayer.currentTime = 0;
				}
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

				TSpan<uint8> fileData = scratch.arena.Push<uint8>(GetFileSize(fileHandle));
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

	state.keys = state.arena.PushZeroed<uint32>((static_cast<size_t>(KeyCode::Count) + 31) / 32);

	ConfigureApp({
		.updateCallback = OnAppUpdate,
		.eventCallback = OnAppEvent,
	});

	GpuInitialize();

	return 0;
}
