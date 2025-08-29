#pragma once

#include "Core/Core.h"
#include "Core/Math.h"
#include "Core/Span.h"
#include "Core/String.h"

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
		String name;
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
			String name;
			int32 parentIdx;
		};

		TSpan<Joint> joints;
		TSpan<Mat4f> invBindPose;
	};

	struct AnimationTrack
	{
		TSpan<float> translationTimes;
		TSpan<Vec3f> translations;

		TSpan<float> rotationTimes;
		TSpan<Quat4f> rotations;

		TSpan<float> scaleTimes;
		TSpan<Vec3f> scales;
	};

	struct Animation
	{
		String name;
		float duration;
		TSpan<AnimationTrack> tracks;
	};

	bool LoadGlbMeshes(Arena& arena, TSpan<uint8> fileData, TSpan<Mesh>& meshes);
	bool LoadGlbSkeletons(Arena& arena, TSpan<uint8> fileData, TSpan<Skeleton>& skeletons);
	bool LoadGlbAnimations(Arena& arena, TSpan<uint8> fileData, const Skeleton& skeleton, TSpan<Animation>& animations);
}
