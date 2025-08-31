#pragma once

#include "Core/Math.h"
#include "Core/Span.h"

#include "AnimSequence.h"
#include "Skeleton.h"

namespace Bk
{
	struct AnimPlayer
	{
		void Update(float deltaTime);

		Skeleton skeleton;
		AnimSequence animation;

		float currentTime;
		TSpan<Mat4f> transforms;
	};
}
