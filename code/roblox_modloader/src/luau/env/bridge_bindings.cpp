#include "RobloxModLoader/luau/env/binding.hpp"

#include "RobloxModLoader/luau/luau_bridge.hpp"
#include "RobloxModLoader/luau/script_host.hpp"
#include "RobloxModLoader/luau/script_runtime.hpp"
#include "RobloxModLoader/luau/vm/stack.hpp"
#include "RobloxModLoader/luau/vm/stack_guard.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"

RML_LOG_SCOPE("BridgeBindings");

namespace rml::luau
{
	static BridgeValue read_bridge_value(lua_State* L, int index, int depth);

	static std::shared_ptr<const BridgeTable> read_bridge_table(lua_State* L, const int index, const int depth)
	{
		auto table = std::make_shared<BridgeTable>();

		const auto absolute = vm::abs_index(L, index);

		lua_pushnil(L);
		while (lua_next(L, absolute) != 0)
		{
			if (lua_type(L, -2) == LUA_TSTRING)
			{
				std::size_t length = 0;
				const auto* key = lua_tolstring(L, -2, &length);
				table->fields.emplace(std::string{key, length}, read_bridge_value(L, -1, depth + 1));
			}

			lua_pop(L, 1);
		}

		return table;
	}

	static BridgeValue read_bridge_value(lua_State* L, const int index, const int depth)
	{
		switch (lua_type(L, index))
		{
		case LUA_TBOOLEAN:
			return lua_toboolean(L, index) != 0;
		case LUA_TNUMBER:
			return lua_tonumberx(L, index, nullptr);
		case LUA_TSTRING:
			return vm::to_string(L, index);
		case LUA_TTABLE:
			if (depth < 8)
			{
				return read_bridge_table(L, index, depth);
			}
			return std::monostate{};
		default:
			return std::monostate{};
		}
	}

	static void push_bridge_value(lua_State* L, const BridgeValue& value)
	{
		std::visit(
		    [L](const auto& held) {
			    using Held = std::decay_t<decltype(held)>;

			    if constexpr (std::is_same_v<Held, bool>)
			    {
				    lua_pushboolean(L, held ? 1 : 0);
			    }
			    else if constexpr (std::is_same_v<Held, double>)
			    {
				    lua_pushnumber(L, held);
			    }
			    else if constexpr (std::is_same_v<Held, std::string>)
			    {
				    vm::push_string(L, held);
			    }
			    else if constexpr (std::is_same_v<Held, std::shared_ptr<const BridgeTable>>)
			    {
				    if (!held)
				    {
					    lua_pushnil(L);
					    return;
				    }

				    lua_createtable(L, 0, static_cast<int>(held->fields.size()));
				    for (const auto& [key, field] : held->fields)
				    {
					    push_bridge_value(L, field);
					    lua_setfield(L, -2, key.c_str());
				    }
			    }
			    else
			    {
				    lua_pushnil(L);
			    }
		    },
		    value);
	}

	static BridgeArgs collect_args(lua_State* L, const int first)
	{
		BridgeArgs args;
		const auto argc = lua_gettop(L);

		for (auto i = first; i <= argc; ++i)
		{
			args.push_back(read_bridge_value(L, i, 0));
		}

		return args;
	}

	static Bridge* bridge_of(lua_State* L)
	{
		auto* runtime = script_runtime();
		return runtime ? &runtime->bridge() : nullptr;
	}

	static int bridge_call(lua_State* L)
	{
		auto* bridge = bridge_of(L);
		if (!bridge)
		{
			luaL_error(L, "bridge.call: the script runtime is not available");
		}

		const auto* mod_name = luaL_checklstring(L, 1, nullptr);
		const auto* function_name = luaL_checklstring(L, 2, nullptr);

		const auto key = std::format("{}.{}", mod_name, function_name);

		const auto target = bridge->find_function(key);
		if (!target)
		{
			luaL_error(L, "bridge.call: '%s' is not registered by any native mod", key.c_str());
		}

		auto results = (*target)(collect_args(L, 3));

		for (const auto& result : results)
		{
			push_bridge_value(L, result);
		}

		return static_cast<int>(results.size());
	}

	static int bridge_emit(lua_State* L)
	{
		auto* bridge = bridge_of(L);
		if (!bridge)
		{
			luaL_error(L, "bridge.emit: the script runtime is not available");
		}

		const auto* event_name = luaL_checklstring(L, 1, nullptr);

		if (auto emitted = bridge->emit(event_name, collect_args(L, 2)); !emitted)
		{
			luaL_error(L, "%s", emitted.error().c_str());
		}

		return 0;
	}

	static int bridge_listen(lua_State* L)
	{
		auto* bridge = bridge_of(L);
		if (!bridge)
		{
			luaL_error(L, "bridge.listen: the script runtime is not available");
		}

		const auto* event_name = luaL_checklstring(L, 1, nullptr);
		luaL_checktype(L, 2, LUA_TFUNCTION);

		auto& env = bound_env(L);
		const auto callback = env.host().retain(anchor_in_env(env, L, 2), std::string{env.mod().mod_name()});

		bridge->add_script_listener(env.host().type(), event_name, callback);

		lua_pushboolean(L, 1);
		return 1;
	}

	static int bridge_set(lua_State* L)
	{
		auto* bridge = bridge_of(L);
		if (!bridge)
		{
			luaL_error(L, "bridge.set: the script runtime is not available");
		}

		const auto* key = luaL_checklstring(L, 1, nullptr);

		if (auto stored = bridge->set_shared(key, read_bridge_value(L, 2, 0)); !stored)
		{
			luaL_error(L, "%s", stored.error().c_str());
		}

		return 0;
	}

	static int bridge_get(lua_State* L)
	{
		auto* bridge = bridge_of(L);
		if (!bridge)
		{
			luaL_error(L, "bridge.get: the script runtime is not available");
		}

		const auto* key = luaL_checklstring(L, 1, nullptr);

		const auto stored = bridge->get_shared(key);
		if (!stored)
		{
			lua_pushnil(L);
			return 1;
		}

		push_bridge_value(L, *stored);
		return 1;
	}

	bool bind_bridge(ScriptEnv& env, lua_State* L) noexcept
	{
		lua_newtable(L);

		push_bound_function(env, L, "call", &bridge_call);
		lua_setfield(L, -2, "call");

		push_bound_function(env, L, "emit", &bridge_emit);
		lua_setfield(L, -2, "emit");

		push_bound_function(env, L, "listen", &bridge_listen);
		lua_setfield(L, -2, "listen");

		push_bound_function(env, L, "set", &bridge_set);
		lua_setfield(L, -2, "set");

		push_bound_function(env, L, "get", &bridge_get);
		lua_setfield(L, -2, "get");

		lua_setreadonly(L, -1, true);
		lua_setglobal(L, "bridge");

		return true;
	}
}
