#pragma once

#include "Core/Core.h"
#include "Core/Span.h"
#include "Core/String.h"

namespace Bk
{
	struct MeshSection
	{
		TSpan<uint8> positionBuffer;
		TSpan<uint8> indexBuffer;
	};

	struct Mesh
	{
		TSpan<MeshSection> sections;
	};

	bool LoadGltfMeshes(Arena& arena, String filePath, TSpan<Mesh>& meshes);
}
