#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace RSL
{
	class Mutex
	{
	public:
		void* handle;
	};

	RML_ASSERT_SIZE(Mutex, sizeof(void*));
}
