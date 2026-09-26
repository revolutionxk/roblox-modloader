#include "RobloxModLoader/luau/vm/vm_guard.hpp"

#include "RobloxModLoader/luau/vm/stack.hpp"

#include <utility>

namespace rml::luau::vm
{
	void raise(lua_State* L, const VmError& error)
	{
		{
			const std::string text = error.describe();
			push_string(L, text);
		}

		lua_error(L);

		std::unreachable();
	}
}
