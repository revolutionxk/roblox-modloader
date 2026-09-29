#include "RobloxModLoader/luau/env/binding.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/luau/extensions/luau_extensions.hpp"
#include "RobloxModLoader/luau/generated/layout_access.hpp"

#include "lapi.h"
#include "ldebug.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"

RML_LOG_SCOPE("DebugBindings");

namespace rml::luau
{
	static void barrier_into(lua_State* L, void* owner, const access::TValue* value)
	{
		if (value->tt < LUA_TSTRING)
		{
			return;
		}

		auto* object = reinterpret_cast<void*>(value->value);

		if (access::needs_barrier(owner, object))
		{
			luaC_barrierf(L, static_cast<GCObject*>(owner), static_cast<GCObject*>(object));
		}
	}

	static void normalize_stack(lua_State* L, const int count)
	{
		if (const int top = lua_gettop(L); top < count)
		{
			for (int i = top; i < count; ++i)
			{
				lua_pushnil(L);
			}
		}
		else if (top > count)
		{
			lua_settop(L, count);
		}
	}

	static int debug_getconstants(lua_State* L)
	{
		luaL_checkany(L, 1);
		normalize_stack(L, 1);

		if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
		{
			luaL_typeerror(L, 1, "Expected function or number for argument #1");
		}

		if (lua_isnumber(L, 1))
		{
			lua_Debug info{};
			const int level = lua_tointeger(L, 1);

			if (!lua_getinfo(L, level, "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			lua_pushvalue(L, 1);
		}

		if (lua_iscfunction(L, -1))
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		const auto* closure = access::closure(luau_to_closure(L, -1));
		const auto* proto = access::proto(closure->p);
		const auto constant_count = proto->sizek;
		const auto* constants = access::value(proto->k);
		auto* state = access::state(L);

		lua_newtable(L);

		for (int i = 0; i < constant_count; i++)
		{
			const auto* constant = access::value_at(constants, i);

			if (constant->tt == LUA_TFUNCTION)
			{
				lua_pushnil(L);
			}
			else
			{
				barrier_into(L, L, constant);
				access::copy_value(state->top, constant);
				access::advance_top(state);
			}

			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}

	static int debug_getconstant(lua_State* L)
	{
		luaL_checkany(L, 2);
		normalize_stack(L, 2);

		if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
		{
			luaL_typeerror(L, 1, "Expected function or number for argument #1");
		}

		const int constant_index = luaL_checkinteger(L, 2);

		if (lua_isnumber(L, 1))
		{
			lua_Debug info{};
			if (!lua_getinfo(L, lua_tointeger(L, 1), "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			lua_pushvalue(L, 1);
		}

		if (lua_iscfunction(L, -1))
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		const auto* closure = access::closure(luau_to_closure(L, -1));
		const auto* proto = access::proto(closure->p);
		const auto* constants = access::value(proto->k);

		if (constant_index < 1)
		{
			luaL_argerror(L, 2, "constant index starts at 1");
		}

		if (constant_index > proto->sizek)
		{
			luaL_argerror(L, 2, "constant index is out of range");
		}

		const auto* constant = access::value_at(constants, constant_index - 1);

		if (constant->tt == LUA_TFUNCTION)
		{
			lua_pushnil(L);
		}
		else
		{
			barrier_into(L, L, constant);

			auto* state = access::state(L);
			access::copy_value(state->top, constant);
			access::advance_top(state);
		}

		return 1;
	}

	static int debug_setconstant(lua_State* L)
	{
		luaL_checkany(L, 1);
		luaL_checknumber(L, 2);
		luaL_checkany(L, 3);

		normalize_stack(L, 3);

		if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
		{
			luaL_typeerror(L, 1, "function or level expected");
		}

		const int index = luaL_checkinteger(L, 2);

		if (lua_isnumber(L, 1))
		{
			lua_Debug info;
			if (!lua_getinfo(L, lua_tointeger(L, 1), "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			lua_pushvalue(L, 1);
		}

		if (lua_iscfunction(L, -1))
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		const auto* closure = access::closure(luau_to_closure(L, -1));
		const auto* proto = access::proto(closure->p);
		auto* constants = access::value(proto->k);

		if (index < 1)
		{
			luaL_argerror(L, 2, "constant index starts at 1");
		}

		if (index > proto->sizek)
		{
			luaL_argerror(L, 2, "constant index out of range");
		}

		auto* constant = access::value_at(constants, index - 1);

		if (constant->tt == LUA_TFUNCTION)
		{
			return 0;
		}

		const auto* new_constant = access::value(luaA_toobject(L, 3));

		if (new_constant->tt != constant->tt)
		{
			luaL_argerror(
			    L, 3, "cannot replace constant when the element you want to replace it with is not of the same type.");
		}

		barrier_into(L, const_cast<access::Proto*>(proto), new_constant);

		access::copy_value(constant, new_constant);

		return 0;
	}

	static int debug_getinfo(lua_State* L)
	{
		luaL_checkany(L, 1);
		normalize_stack(L, 1);
		auto info_level = 0;

		if (lua_isnumber(L, 1))
		{
			info_level = static_cast<int>(lua_tointeger(L, 1));
			luaL_argcheck(L, info_level >= 0, 1, "level cannot be negative");
		}
		else if (lua_isfunction(L, 1))
		{
			info_level = -lua_gettop(L);
		}
		else
		{
			luaL_argerror(L, 1, "function or level expected");
		}

		alignas(16) std::array<std::byte, 1024> storage{};
		const auto* record = access::debug_record(storage.data());

		if (!lua_getinfo(L, info_level, "fulasn", reinterpret_cast<lua_Debug*>(storage.data())))
		{
			luaL_argerror(L, 1, "invalid level");
		}

		const auto text = [](const char* value) { return value != nullptr ? value : ""; };

		lua_newtable(L);

		lua_pushstring(L, text(record->source));
		lua_setfield(L, -2, "source");

		lua_pushstring(L, text(record->short_src));
		lua_setfield(L, -2, "short_src");

		lua_pushvalue(L, 1);
		lua_setfield(L, -2, "func");

		lua_pushstring(L, text(record->what));
		lua_setfield(L, -2, "what");

		lua_pushinteger(L, record->currentline);
		lua_setfield(L, -2, "currentline");

		lua_pushinteger(L, record->linedefined);
		lua_setfield(L, -2, "linedefined");

		lua_pushstring(L, text(record->name));
		lua_setfield(L, -2, "name");

		lua_pushinteger(L, record->nupvals);
		lua_setfield(L, -2, "nups");

		lua_pushinteger(L, record->nparams);
		lua_setfield(L, -2, "numparams");

		lua_pushinteger(L, record->isvararg);
		lua_setfield(L, -2, "is_vararg");

		return 1;
	}

	static int debug_getproto(lua_State* L)
	{
		luaL_checkany(L, 1);
		luaL_checktype(L, 2, LUA_TNUMBER);
		const bool active = luaL_optboolean(L, 3, true);
		normalize_stack(L, 3);

		if (!active)
		{
			luaL_argerror(L, 3, "prototypes cannot be inactive (not implemented)");
		}

		if (!lua_isnumber(L, 1) && !lua_isfunction(L, 1))
		{
			luaL_argerror(L, 1, "function or level expected");
		}

		if (lua_isnumber(L, 1))
		{
			const int level = static_cast<int>(lua_tointeger(L, 1));
			lua_Debug info;
			if (!lua_getinfo(L, level, "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			luaL_checktype(L, 1, LUA_TFUNCTION);
			lua_pushvalue(L, 1);
		}

		if (lua_iscfunction(L, -1))
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		luaL_error(L, "debug.getproto needs Proto.nups, which the dumper has not recovered yet");
	}

	static int debug_getprotos(lua_State* L)
	{
		luaL_checkany(L, 1);
		normalize_stack(L, 1);

		if (!lua_isnumber(L, 1) && !lua_isfunction(L, 1))
		{
			luaL_argerror(L, 1, "function or level expected");
		}

		if (lua_isnumber(L, 1))
		{
			const int level = static_cast<int>(lua_tointeger(L, 1));
			lua_Debug info;
			if (!lua_getinfo(L, level, "f", &info))
			{
				luaL_error(L, "level out of range");
			}
		}
		else
		{
			luaL_checktype(L, 1, LUA_TFUNCTION);
			lua_pushvalue(L, 1);
		}

		if (lua_iscfunction(L, -1))
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		luaL_error(L, "debug.getprotos needs Proto.nups, which the dumper has not recovered yet");
	}

	static int debug_setstack(lua_State* L)
	{
		luaL_checktype(L, 1, LUA_TNUMBER);
		luaL_checktype(L, 2, LUA_TNUMBER);
		luaL_checkany(L, 3);
		normalize_stack(L, 3);

		const auto level = lua_tointeger(L, 1);
		const auto index = lua_tointeger(L, 2);

		auto* state = access::state(L);

		if (level >= access::frames_between(state->ci, state->base_ci) || level < 0)
		{
			luaL_argerror(L, 1, "level out of range");
		}

		auto* frame = access::frame_at(state->ci, -static_cast<int>(level));
		const auto stack_size = frame->top - frame->base;

		if (access::closure_in(frame->func)->isC != 0)
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		if (index < 1 || index > stack_size)
		{
			luaL_argerror(L, 2, "stack index out of range");
		}

		auto* slot = access::value_at(frame->base, static_cast<int>(index) - 1);

		if (slot->tt != lua_type(L, 3))
		{
			luaL_argerror(L, 2, "type on the stack is different than that you are trying to set!");
		}

		const auto* replacement = access::value(luaA_toobject(L, 3));

		barrier_into(L, L, replacement);

		access::copy_value(slot, replacement);
		return 0;
	}

	static int debug_getstack(lua_State* L)
	{
		luaL_checktype(L, 1, LUA_TNUMBER);

		const auto level = lua_tointeger(L, 1);
		const auto index = luaL_optinteger(L, 2, 69420);
		normalize_stack(L, 2);

		auto* state = access::state(L);

		if (level >= access::frames_between(state->ci, state->base_ci) || level < 0)
		{
			luaL_argerror(L, 1, "level out of range");
		}

		auto* frame = access::frame_at(state->ci, -static_cast<int>(level));
		const auto frame_size = static_cast<int>(frame->top - frame->base);

		if (access::closure_in(frame->func)->isC != 0)
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		if (index == 69420)
		{
			lua_newtable(L);

			for (int i = 0; i < frame_size; i++)
			{
				access::copy_value(state->top, access::value_at(frame->base, i));
				access::advance_top(state);
				lua_rawseti(L, -2, i + 1);
			}
		}
		else
		{
			if (index < 1 || index > frame_size)
			{
				luaL_argerror(L, 2, "index out of range");
			}

			access::copy_value(state->top, access::value_at(frame->base, static_cast<int>(index) - 1));
			access::advance_top(state);
		}

		return 1;
	}

	static int debug_setupvalue(lua_State* L)
	{
		const int index = luaL_checkinteger(L, 2);
		luaL_checkany(L, 3);
		normalize_stack(L, 3);

		if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
		{
			luaL_typeerror(L, 1, "function or level expected");
		}

		if (lua_isnumber(L, 1))
		{
			lua_Debug info;
			if (!lua_getinfo(L, lua_tointeger(L, 1), "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			lua_pushvalue(L, 1);
		}

		if (lua_iscfunction(L, -1))
		{
			luaL_argerror(L, 1, "Lua function expected.");
		}

		auto* raw = access::closure_in(access::value(luaA_toobject(L, -1)));
		auto* closure = access::closure(raw);
		const auto* value = access::value(luaA_toobject(L, 3));
		auto* upvalues = access::upvalues_of(closure);

		if (index < 1)
		{
			luaL_argerror(L, 2, "upvalue index starts at 1");
		}

		if (index > closure->nupvalues)
		{
			luaL_argerror(L, 2, "upvalue index out of range");
		}

		auto* upvalue = access::value_at(upvalues, index - 1);

		barrier_into(L, raw, value);

		access::copy_value(upvalue, value);

		lua_pushboolean(L, true);
		return 1;
	}

	static int debug_getupvalue(lua_State* L)
	{
		luaL_checktype(L, 2, LUA_TNUMBER);
		normalize_stack(L, 2);

		if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
		{
			luaL_typeerror(L, 1, "function or level expected");
		}

		if (lua_isnumber(L, 1))
		{
			lua_Debug info;
			if (!lua_getinfo(L, lua_tointeger(L, 1), "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			lua_pushvalue(L, 1);
		}

		const int index = luaL_checkinteger(L, 2);

		const auto* closure = access::closure_in(access::value(luaA_toobject(L, -1)));
		const auto* upvalues = access::upvalues_of(closure);

		if (!index)
		{
			luaL_argerror(L, 2, "upvalue index starts at 1");
		}

		if (index > closure->nupvalues)
		{
			luaL_argerror(L, 2, "upvalue index is out of range");
		}

		const auto* upvalue = access::value_at(upvalues, index - 1);

		barrier_into(L, L, upvalue);

		auto* state = access::state(L);
		access::copy_value(state->top, upvalue);
		access::advance_top(state);

		return 1;
	}

	static int debug_getupvalues(lua_State* L)
	{
		normalize_stack(L, 1);

		if (!lua_isfunction(L, 1) && !lua_isnumber(L, 1))
		{
			luaL_typeerror(L, 1, "function or level expected");
		}

		if (lua_isnumber(L, 1))
		{
			lua_Debug info;
			if (!lua_getinfo(L, lua_tointeger(L, 1), "f", &info))
			{
				luaL_argerror(L, 1, "level out of range");
			}
		}
		else
		{
			lua_pushvalue(L, 1);
		}

		const auto* closure = access::closure_in(access::value(luaA_toobject(L, -1)));
		const auto* upvalues = access::upvalues_of(closure);
		auto* state = access::state(L);

		lua_newtable(L);

		for (int i = 0; i < closure->nupvalues; i++)
		{
			const auto* upvalue = access::value_at(upvalues, i);

			barrier_into(L, L, upvalue);
			access::copy_value(state->top, upvalue);
			access::advance_top(state);
			lua_rawseti(L, -2, i + 1);
		}

		return 1;
	}

	bool bind_debug(ScriptEnv& env, lua_State* L) noexcept
	{
		struct Entry
		{
			const char* name;
			lua_CFunction fn;
		};

		static constexpr std::array entries{
		    Entry{"getconstants", &debug_getconstants},
		    Entry{"getconstant", &debug_getconstant},
		    Entry{"setconstant", &debug_setconstant},
		    Entry{"getinfo", &debug_getinfo},
		    Entry{"getproto", &debug_getproto},
		    Entry{"getprotos", &debug_getprotos},
		    Entry{"setstack", &debug_setstack},
		    Entry{"getstack", &debug_getstack},
		    Entry{"setupvalue", &debug_setupvalue},
		    Entry{"getupvalue", &debug_getupvalue},
		    Entry{"getupvalues", &debug_getupvalues},
		};

		lua_getglobal(L, "debug");

		if (!lua_istable(L, -1))
		{
			RML_ERROR("The engine left no debug table to merge into for mod '{}'", env.mod().mod_name());
			return false;
		}

		lua_setreadonly(L, -1, false);

		for (const auto& [name, fn] : entries)
		{
			push_bound_function(env, L, name, fn);
			lua_setfield(L, -2, name);
		}

		return true;
	}
}
