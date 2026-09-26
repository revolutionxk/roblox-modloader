#include "RobloxModLoader/luau/vm/vm_error.hpp"

#include "RobloxModLoader/luau/vm/stack.hpp"
#include "RobloxModLoader/luau/vm/vm_api.hpp"

namespace rml::luau::vm
{
	static constexpr char kMessageField[] = "message";
	static constexpr char kTracebackField[] = "traceback";

	std::string capture_traceback(lua_State* L)
	{
		const int base = lua_gettop(L);
		std::string text;

		lua_getglobal(L, "debug");
		if (lua_istable(L, -1))
		{
			lua_getfield(L, -1, "traceback");
			if (lua_isfunction(L, -1))
			{
				lua_pushnil(L);
				lua_pushinteger(L, 2);

				if (lua_pcall(L, 2, 1, 0) == LUA_OK)
				{
					std::size_t length = 0;
					if (const char* rendered = lua_tolstring(L, -1, &length))
					{
						text.assign(rendered, length);
					}
				}
			}
		}

		lua_settop(L, base);
		return text;
	}

	static int traceback_handler(lua_State* L)
	{
		const char* raw = lua_tostring(L, 1);
		const std::string message = raw ? raw : kUnknownError;
		const std::string traceback = capture_traceback(L);

		lua_createtable(L, 0, 2);
		push_string(L, message);
		lua_setfield(L, -2, kMessageField);
		push_string(L, traceback);
		lua_setfield(L, -2, kTracebackField);

		return 1;
	}

	static void drop_stack_slot(lua_State* L, const int index)
	{
		const int above = lua_gettop(L) - index;
		for (int i = 0; i < above; ++i)
		{
			lua_insert(L, index);
		}

		lua_pop(L, 1);
	}

	static bool read_field(lua_State* L, const char* field, std::string& out)
	{
		lua_getfield(L, -1, field);

		std::size_t length = 0;
		const char* raw = lua_tolstring(L, -1, &length);
		if (raw)
		{
			out.assign(raw, length);
		}

		lua_pop(L, 1);
		return raw != nullptr;
	}

	static VmError error_from_handler_result(lua_State* L, const VmError::Kind kind)
	{
		if (lua_type(L, -1) != LUA_TTABLE)
		{
			return error_from_stack(L, kind);
		}

		VmError error{.kind = kind};
		if (!read_field(L, kMessageField, error.message))
		{
			error.message = kUnknownError;
		}
		read_field(L, kTracebackField, error.traceback);
		lua_pop(L, 1);

		return error;
	}

	VmError::Kind kind_from_status(const int status) noexcept
	{
		switch (status)
		{
			case LUA_ERRSYNTAX:
				return VmError::Kind::Syntax;
			case LUA_ERRMEM:
				return VmError::Kind::Memory;
			case LUA_ERRRUN:
				return VmError::Kind::Runtime;
			default:
				return VmError::Kind::Internal;
		}
	}

	VmError error_from_stack(lua_State* L, const VmError::Kind kind)
	{
		VmError error{.kind = kind};

		if (!L)
		{
			error.message = "no Luau state";
			return error;
		}

		std::size_t length = 0;
		if (const char* raw = lua_tolstring(L, -1, &length))
		{
			error.message.assign(raw, length);
		}
		else
		{
			error.message = kUnknownError;
		}

		lua_pop(L, 1);
		return error;
	}

	std::expected<int, VmError> protected_call(lua_State* L, const int nargs, const int nresults) noexcept
	{
		if (!L)
		{
			return std::unexpected(VmError::internal("protected_call without a Luau state"));
		}

		if (!api_ready())
		{
			report_api_unavailable_once();
			return std::unexpected(VmError::unavailable("the Luau C API is unavailable on this Studio build"));
		}

		const int handler_index = lua_gettop(L) - nargs;
		if (handler_index < 1)
		{
			return std::unexpected(VmError::internal(std::format("protected_call with {} arguments but only {} stack slots", nargs, lua_gettop(L))));
		}

		lua_pushcfunction(L, &traceback_handler, "rml_traceback");
		lua_insert(L, handler_index);

		const int status = lua_pcall(L, nargs, nresults, handler_index);

		if (status != LUA_OK)
		{
			drop_stack_slot(L, handler_index);
			return std::unexpected(error_from_handler_result(L, kind_from_status(status)));
		}

		const int produced = lua_gettop(L) - handler_index;
		drop_stack_slot(L, handler_index);

		return produced;
	}
}
