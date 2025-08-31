#pragma once

#include "Core/Core.h"
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
		TSpan<uint8> positionBuffer;
		TSpan<uint8> jointIndexBuffer;
		TSpan<uint8> jointWeightBuffer;
		TSpan<uint8> indexBuffer;
	};

	struct Mesh
	{
		TSpan<MeshSection> sections;
		uint32 vertexBuffer;
		uint32 jointIndexBuffer;
		uint32 jointWeightBuffer;
		uint32 indexBuffer;
	};
}
