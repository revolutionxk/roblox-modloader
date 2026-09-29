#pragma once

#include "RobloxModLoader/internal/platform.hpp"

#include <cstddef>

namespace rml::platform::abi
{
#if defined(RML_WINDOWS)
	inline constexpr std::size_t vtable_prefix_slots = 0;
	inline constexpr std::size_t destructor_slots = 1;
	inline constexpr std::size_t scalar_deleting_destructor_slot = 0;
	inline constexpr std::size_t destructor_body_slot = scalar_deleting_destructor_slot;
	inline constexpr bool returns_via_hidden_pointer = true;
	inline constexpr bool callee_destroys_arguments = true;
#else
	inline constexpr std::size_t vtable_prefix_slots = 2;
	inline constexpr std::size_t destructor_slots = 2;
	inline constexpr std::size_t complete_destructor_slot = 0;
	inline constexpr std::size_t deleting_destructor_slot = 1;
	inline constexpr std::size_t destructor_body_slot = complete_destructor_slot;
	inline constexpr bool returns_via_hidden_pointer = false;
	inline constexpr bool callee_destroys_arguments = false;
#endif
}
