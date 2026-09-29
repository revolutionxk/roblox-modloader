#pragma once

#include "small_vector.hpp"

#include "RobloxModLoader/util/layout_assert.hpp"

namespace RBX
{
	class NumberSequenceKeypoint
	{
	public:
		float time;
		float value;
		float envelope;
	};

	class NumberSequence
	{
	public:
		using Keypoint = NumberSequenceKeypoint;

		SmallVector<NumberSequenceKeypoint, 4> keypoints;
	};

	RML_ASSERT_SIZE(NumberSequenceKeypoint, 12);
	RML_ASSERT_SIZE(NumberSequence, 0x40);
}
