#pragma once

#include "Core/Core.h"
#include "Core/Math.h"
#include "Core/Span.h"
#include "Core/String.h"

namespace Bk
{
	struct Bone
	{
		String name;
		int32 parentIdx;
	};

	struct Skeleton
	{
		String name;
		TSpan<Bone> bones;
		TSpan<Mat4f> invBindPose;
	};
}
