#pragma once

#include "Core/Math.h"
#include "Core/Span.h"

#include "AnimSequence.h"
#include "Skeleton.h"

namespace Bk
{
	struct AnimPlayer
	{
		Skeleton skeleton;
		AnimSequence animation;

		float currentTime;
		TSpan<Mat4f> transforms;
	};

	void UpdateAnimation(AnimPlayer& player, float deltaTime);
}
