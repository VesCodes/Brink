#pragma once

#include "Core/Core.h"
#include "Core/Span.h"
#include "Core/String.h"

#define HANDMADE_MATH_USE_DEGREES
#include <HandmadeMath.h>

namespace Bk
{
	struct MeshSection
	{
		size_t vertexOffset;
		size_t indexOffset;
		size_t triangleCount;
	};

	struct Mesh
	{
		TSpan<MeshSection> sections;
		TSpan<uint8> positionBuffer;
		TSpan<uint8> jointIndexBuffer;
		TSpan<uint8> jointWeightBuffer;
		TSpan<uint8> indexBuffer;
	};

	struct Skeleton
	{
		struct Joint
		{
			int32 parentIdx;
			String name;
		};

		TSpan<Joint> joints;
		TSpan<HMM_Mat4> invBindPose;
	};

	struct AnimationTrack
	{
		TSpan<float> translationTimes;
		TSpan<HMM_Vec3> translations;

		TSpan<float> rotationTimes;
		TSpan<HMM_Quat> rotations;

		TSpan<float> scaleTimes;
		TSpan<HMM_Vec3> scales;
	};

	struct Animation
	{
		TSpan<AnimationTrack> tracks;
	};

	bool LoadGlbMeshes(Arena& arena, TSpan<uint8> fileData, TSpan<Mesh>& meshes);
	bool LoadGlbSkeletons(Arena& arena, TSpan<uint8> fileData, TSpan<Skeleton>& skeletons);
	bool LoadGlbAnimations(Arena& arena, TSpan<uint8> fileData, const Skeleton& skeleton, TSpan<Animation>& animations);
}
