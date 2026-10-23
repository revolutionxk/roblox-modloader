#pragma once

#include "RobloxModLoader/util/layout_assert.hpp"

namespace RSL
{
	class Mutex
	{
	public:
		void* handle;

		void lock();
		void unlock();
	};

	RML_ASSERT_SIZE(Mutex, sizeof(void*));
}
