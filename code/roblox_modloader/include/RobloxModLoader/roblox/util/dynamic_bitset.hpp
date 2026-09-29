#pragma once

#include "RobloxModLoader/roblox/rsl/arena.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace RBX
{
	class dynamic_bitset
	{
	public:
		RSL::Arena* arena;
		std::uint64_t* words;
		std::uint32_t size;
		std::uint32_t capacity;
	};

	RML_ASSERT_SIZE(dynamic_bitset, 0x18);
}
