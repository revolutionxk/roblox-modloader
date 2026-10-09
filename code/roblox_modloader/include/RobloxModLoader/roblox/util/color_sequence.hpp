#pragma once

#include "small_vector.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"
#include "RobloxModLoader/roblox/util/G3DCore.h"

namespace RBX
{
	class ColorSequenceKeypoint
	{
	public:
		float time;
		Color3 value;
		float envelope;
	};

	class ColorSequence
	{
	public:
		using Keypoint = ColorSequenceKeypoint;

		SmallVector<ColorSequenceKeypoint, 2> keypoints;
	};

	RML_ASSERT_SIZE(ColorSequenceKeypoint, 20);
	RML_ASSERT_SIZE(ColorSequence, 0x38);
}
