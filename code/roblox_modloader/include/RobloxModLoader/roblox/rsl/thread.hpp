#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace RSL
{
	class Thread
	{
	public:
		void* handle;
	};

	RML_ASSERT_SIZE(Thread, sizeof(void*));
}
