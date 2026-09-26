#pragma once

#include "RobloxModLoader/internal/common.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace rml::luau::vm
{
	[[nodiscard]] inline int abs_index(lua_State* L, const int index) noexcept
	{
		return index < 0 && index > LUA_REGISTRYINDEX ? lua_gettop(L) + 1 + index : index;
	}

	inline void push_string(lua_State* L, const std::string_view text)
	{
		lua_pushlstring(L, text.data(), text.size());
	}

	[[nodiscard]] inline std::string to_string(lua_State* L, const int index)
	{
		std::size_t length = 0;
		const auto* text = lua_tolstring(L, index, &length);
		return text ? std::string{text, length} : std::string{};
	}
}
