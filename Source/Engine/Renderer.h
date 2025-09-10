#pragma once

#include "Mesh.h"

namespace Bk
{
	void InitializeRenderer();

	uint32 CreateMesh(const MeshDesc& desc);
	void DestroyMesh(uint32 handle);

	void SkinMesh(uint32 handle, TSpan<Mat4f> boneTransforms);
	void DrawMesh(uint32 handle, Mat4f mvp);
}
