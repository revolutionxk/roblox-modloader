#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace RBX
{
	class Faces
	{
	public:
		std::int32_t normal_mask;
	};

	class Axes
	{
	public:
		std::int32_t axis_mask;
	};

	RML_ASSERT_SIZE(Faces, 4);
	RML_ASSERT_SIZE(Axes, 4);
}
