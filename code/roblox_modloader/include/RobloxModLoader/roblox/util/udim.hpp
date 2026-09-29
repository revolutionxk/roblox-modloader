#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace RBX
{
	class UDim
	{
	public:
		float scale;
		std::int32_t offset;
	};

	class UDim2
	{
	public:
		UDim x;
		UDim y;
	};

	RML_ASSERT_SIZE(UDim, 8);
	RML_ASSERT_SIZE(UDim2, 16);
}
