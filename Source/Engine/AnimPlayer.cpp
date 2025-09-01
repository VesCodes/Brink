#include "AnimPlayer.h"

namespace Bk
{
	static size_t GetKeyframe(TSpan<float> times, float t)
	{
		size_t lowerBound = 0;
		size_t upperBound = times.length - 1;

		while (lowerBound < upperBound)
		{
			size_t mid = (lowerBound + upperBound) / 2;
			if (times[mid] < t)
			{
				lowerBound = mid + 1;
			}
			else
			{
				upperBound = mid;
			}
		}

		return lowerBound;
	}

	void AnimPlayer::Update(float deltaTime)
	{
		currentTime += deltaTime;
		if (currentTime > animation.duration)
		{
			currentTime -= animation.duration;
		}

		for (size_t i = 0; i < transforms.length; ++i)
		{
			const Bone& bone = skeleton.bones[i];
			const AnimTrack& track = animation.tracks[i];
			Mat4f& transform = transforms[i];

			transform = Mat4f::Identity;

			if (track.scaleTimes.length != 0)
			{
				size_t keyframe = GetKeyframe(track.scaleTimes, currentTime);
				Vec3f scale = track.scales[keyframe];

				if (keyframe > 0)
				{
					float t0 = track.scaleTimes[keyframe - 1];
					float t1 = track.scaleTimes[keyframe];
					float alpha = (currentTime - t0) / (t1 - t0);

					scale = Lerp(track.scales[keyframe - 1], scale, alpha);
				}

				transform = ScaleMatrix(scale);
			}

			if (track.rotationTimes.length != 0)
			{
				size_t keyframe = GetKeyframe(track.rotationTimes, currentTime);
				Quat4f rotation = track.rotations[keyframe];

				if (keyframe > 0)
				{
					float t0 = track.rotationTimes[keyframe - 1];
					float t1 = track.rotationTimes[keyframe];
					float alpha = (currentTime - t0) / (t1 - t0);

					if (Dot(track.rotations[keyframe - 1], rotation) < 0)
					{
						rotation.x = -rotation.x;
						rotation.y = -rotation.y;
						rotation.z = -rotation.z;
						rotation.w = -rotation.w;
					}

					rotation = Lerp(track.rotations[keyframe - 1], rotation, alpha);
				}

				transform = RotationMatrix(rotation) * transform;
			}

			if (track.translationTimes.length != 0)
			{
				size_t keyframe = GetKeyframe(track.translationTimes, currentTime);
				Vec3f translation = track.translations[keyframe];

				if (keyframe > 0)
				{
					float t0 = track.translationTimes[keyframe - 1];
					float t1 = track.translationTimes[keyframe];
					float alpha = (currentTime - t0) / (t1 - t0);

					translation = Lerp(track.translations[keyframe - 1], translation, alpha);
				}

				transform = TranslationMatrix(translation) * transform;
			}

			if (bone.parentIdx != -1)
			{
				transform = transforms[bone.parentIdx] * transform;
			}
		}

		for (size_t i = 0; i < transforms.length; ++i)
		{
			transforms[i] = transforms[i] * skeleton.invBindPose[i];
		}
	}
}
