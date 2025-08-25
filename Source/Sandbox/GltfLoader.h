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
		TSpan<uint8> boneIndexBuffer;
		TSpan<uint8> boneWeightBuffer;
		TSpan<uint8> indexBuffer;
		TSpan<HMM_Mat4> invBindPose;
	};

	bool LoadGlbMeshes(Arena& arena, String filePath, TSpan<Mesh>& meshes);
	bool LoadGlbMeshes(Arena& arena, TSpan<uint8> fileData, TSpan<Mesh>& meshes);
}
