#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace RBX
{
	class NumberRange
	{
	public:
		float min;
		float max;
	};

	RML_ASSERT_SIZE(NumberRange, 8);
}
