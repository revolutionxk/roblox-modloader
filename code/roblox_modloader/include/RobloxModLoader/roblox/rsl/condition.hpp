#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace RSL
{
	class Condition
	{
	public:
		void* handle;
	};

	RML_ASSERT_SIZE(Condition, sizeof(void*));
}
