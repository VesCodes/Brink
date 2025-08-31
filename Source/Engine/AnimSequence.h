#pragma once

#include "Core/Math.h"
#include "Core/Span.h"
#include "Core/String.h"

namespace Bk
{
	struct AnimTrack
	{
		TSpan<float> translationTimes;
		TSpan<Vec3f> translations;

		TSpan<float> rotationTimes;
		TSpan<Quat4f> rotations;

		TSpan<float> scaleTimes;
		TSpan<Vec3f> scales;
	};

	struct AnimSequence
	{
		String name;
		float duration;
		TSpan<AnimTrack> tracks;
	};
}
