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
		TSpan<Vec2f> texCoords;
		TSpan<uint16> boneIndices;
		TSpan<float> boneWeights;
		TSpan<uint16> indices;
	};
}
