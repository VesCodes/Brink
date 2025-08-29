#include "Core/Application.h"
#include "Core/Core.h"
#include "Core/Math.h"
#include "Core/Memory.h"
#include "Core/Platform.h"
#include "Core/String.h"

#include "Renderer/Gpu.h"

#include "GltfLoader.h"

using namespace Bk;

struct MeshProxy
{
	TSpan<MeshSection> sections;
	uint32 vertexBuffer;
	uint32 jointIndexBuffer;
	uint32 jointWeightBuffer;
	uint32 indexBuffer;
};

struct AnimationPlayer
{
	Skeleton skeleton;
	Animation animation;

	float currentTime;
	TSpan<Mat4f> transforms;
};

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

	TSpan<MeshProxy> meshProxies;
	TSpan<Animation> animations;
	AnimationPlayer animPlayer;
	size_t activeAnimation;

	Vec3f cameraPosition;
	Quat4f cameraOrientation;
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
	state.animations = {};
	state.animPlayer = {};

	StringBuilder resourceNameBuilder(scratch.arena);

	TSpan<Mesh> meshes = {};
	if (LoadGlbMeshes(scratch.arena, TSpan(data, length), meshes))
	{
		state.meshProxies = state.arena.Push<MeshProxy>(meshes.length);
		for (size_t meshIdx = 0; meshIdx < state.meshProxies.length; ++meshIdx)
		{
			const Mesh& mesh = meshes[meshIdx];
			MeshProxy& meshProxy = state.meshProxies[meshIdx];

			meshProxy.sections = state.arena.Copy(mesh.sections);

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_VertexBuffer", meshIdx);

			meshProxy.vertexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = mesh.positionBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_JointIndexBuffer", meshIdx);

			meshProxy.jointIndexBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = mesh.jointIndexBuffer,
			});

			resourceNameBuilder.Reset();
			resourceNameBuilder.Appendf("Mesh%02d_JointWeightBuffer", meshIdx);

			meshProxy.jointWeightBuffer = CreateBuffer({
				.name = resourceNameBuilder.ToString(scratch.arena),
				.type = GpuBufferType::Vertex,
				.data = mesh.jointWeightBuffer,
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

	TSpan<Skeleton> skeletons = {};
	if (LoadGlbSkeletons(scratch.arena, TSpan(data, length), skeletons) && skeletons.length > 0)
	{
		state.animPlayer.skeleton = skeletons[0];

		state.animPlayer.skeleton.joints = state.arena.Copy(state.animPlayer.skeleton.joints);
		for (Skeleton::Joint& joint : state.animPlayer.skeleton.joints)
		{
			joint.name = state.arena.Copy(joint.name);
		}

		state.animPlayer.skeleton.invBindPose = state.arena.Copy(state.animPlayer.skeleton.invBindPose);
		state.animPlayer.transforms = state.arena.Push<Mat4f>(state.animPlayer.skeleton.joints.length);

		LoadGlbAnimations(state.arena, TSpan(data, length), state.animPlayer.skeleton, state.animations);
		if (state.animations.length > 0)
		{
			state.activeAnimation = 0;
			state.animPlayer.animation = state.animations[state.activeAnimation];
			state.animPlayer.currentTime = 0;
		}
	}
}

size_t GetKeyframe(TSpan<float> times, float t)
{
	size_t lowerBound = 0;
	size_t upperBound = times.length - 1;

	while (lowerBound < upperBound)
	{
		size_t mid = (lowerBound + upperBound) / 2;
		if (times[mid] < t)
		{
			lowerBound = mid + 1;
		}
		else
		{
			upperBound = mid;
		}
	}

	return lowerBound;
}

void UpdateAnimation(AnimationPlayer& player, float deltaTime)
{
	player.currentTime += deltaTime;
	if (player.currentTime > player.animation.duration)
	{
		player.currentTime -= player.animation.duration;
	}

	for (size_t i = 0; i < player.transforms.length; ++i)
	{
		const Skeleton::Joint& joint = player.skeleton.joints[i];
		const AnimationTrack& track = player.animation.tracks[i];
		Mat4f& transform = player.transforms[i];

		transform = Mat4f::Identity;

		if (track.scaleTimes.length > 0)
		{
			size_t keyframe = GetKeyframe(track.scaleTimes, player.currentTime);
			Vec3f scale = track.scales[keyframe];

			if (keyframe > 0)
			{
				float t0 = track.scaleTimes[keyframe - 1];
				float t1 = track.scaleTimes[keyframe];
				float alpha = (player.currentTime - t0) / (t1 - t0);

				scale = Lerp(track.scales[keyframe - 1], scale, alpha);
			}

			transform = ScaleMatrix(scale);
		}

		if (track.rotationTimes.length > 0)
		{
			size_t keyframe = GetKeyframe(track.rotationTimes, player.currentTime);
			Quat4f rotation = track.rotations[keyframe];

			if (keyframe > 0)
			{
				float t0 = track.rotationTimes[keyframe - 1];
				float t1 = track.rotationTimes[keyframe];
				float alpha = (player.currentTime - t0) / (t1 - t0);

				if (Dot(track.rotations[keyframe - 1], rotation) < 0)
				{
					rotation.x = -rotation.x;
					rotation.y = -rotation.y;
					rotation.z = -rotation.z;
					rotation.w = -rotation.w;
				}

				rotation = Lerp(track.rotations[keyframe - 1], rotation, alpha);
			}

			transform = RotationMatrix(rotation) * transform;
		}

		if (track.translationTimes.length > 0)
		{
			size_t keyframe = GetKeyframe(track.translationTimes, player.currentTime);
			Vec3f translation = track.translations[keyframe];

			if (keyframe > 0)
			{
				float t0 = track.translationTimes[keyframe - 1];
				float t1 = track.translationTimes[keyframe];
				float alpha = (player.currentTime - t0) / (t1 - t0);

				translation = Lerp(track.translations[keyframe - 1], translation, alpha);
			}

			transform = TranslationMatrix(translation) * transform;
		}

		if (joint.parentIdx != -1)
		{
			transform = player.transforms[joint.parentIdx] * transform;
		}
	}

	for (size_t i = 0; i < player.transforms.length; ++i)
	{
		player.transforms[i] = player.transforms[i] * player.skeleton.invBindPose[i];
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
	double deltaTime = currentTime - state.lastTime;

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
			state.cameraPosition.y -= state.cameraSpeed;
		}

		if (IsKeyDown(KeyCode::E))
		{
			state.cameraPosition.y += state.cameraSpeed;
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
			state.cameraPosition += RotateVector(state.cameraOrientation, cameraMove) * state.cameraSpeed;
		}
	}

	Mat4f proj = PerspectiveMatrix(30.0f * (3.14f / 180.0f), 16.0f / 9.0f, 0.01f, 100.0f);
	Mat4f view = RotationMatrix(Conjugate(state.cameraOrientation)) * TranslationMatrix(-state.cameraPosition);
	Mat4f model = Mat4f::Identity;

	Mat4f mvp = proj * view * model;

	WriteBuffer(state.globalsBuffer, TSpan((uint8*)mvp.elements, sizeof(Mat4f)));

	if (state.animPlayer.transforms.length != 0)
	{
		UpdateAnimation(state.animPlayer, static_cast<float>(deltaTime));
		WriteBuffer(state.jointTransformsBuffer, TSpan((uint8*)state.animPlayer.transforms.data, sizeof(Mat4f) * state.animPlayer.transforms.length));
	}

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
				.pipeline = state.meshPipeline,
				.vertexBuffers = {
					meshProxy.vertexBuffer,
					meshProxy.jointIndexBuffer,
					meshProxy.jointWeightBuffer,
				},
				.indexBuffer = meshProxy.indexBuffer,
				.bindingGroups = {
					state.globalBindingGroup,
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
				BitsetSet(state.keys, size_t(appEvent.keyCode));
			}
			else
			{
				BitsetUnset(state.keys, size_t(appEvent.keyCode));

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
