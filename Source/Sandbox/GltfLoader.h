#pragma once

#include "Core/Span.h"

namespace Bk
{
	struct Arena;
	struct MeshDesc;
	struct Skeleton;
	struct AnimSequence;

	bool LoadGlbMeshes(Arena& arena, TSpan<uint8> fileData, TSpan<MeshDesc>& meshes);
	bool LoadGlbSkeletons(Arena& arena, TSpan<uint8> fileData, TSpan<Skeleton>& skeletons);
	bool LoadGlbAnimations(Arena& arena, TSpan<uint8> fileData, const Skeleton& skeleton, TSpan<AnimSequence>& animations);
}
