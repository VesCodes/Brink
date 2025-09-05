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

	struct MeshDesc
	{
		String name;
		TSpan<MeshSection> sections;
		TSpan<Vec3f> positions;
		TSpan<uint8> boneIndices;
		TSpan<float> boneWeights;
		TSpan<uint16> indices;
	};

	struct Mesh
	{
		TSpan<MeshSection> sections;
		uint32 positionsBuffer;
		uint32 boneIndicesBuffer;
		uint32 boneWeightsBuffer;
		uint32 indicesBuffer;
	};
}
