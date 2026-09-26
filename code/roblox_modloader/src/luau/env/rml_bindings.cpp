#include "RobloxModLoader/assets/roblox_mesh.hpp"
#include "RobloxModLoader/assets/temporary_assets.hpp"
#include "RobloxModLoader/luau/env/binding.hpp"
#include "RobloxModLoader/luau/script_host.hpp"
#include "RobloxModLoader/luau/vm/stack.hpp"

#include <filesystem>

RML_LOG_SCOPE("LuauMod");

namespace rml::luau
{
	static std::string formatted_message(lua_State* L)
	{
		const auto argc = lua_gettop(L);

		std::size_t length = 0;
		const auto* first = luaL_checklstring(L, 1, &length);

		if (argc <= 1)
		{
			return std::string{first, length};
		}

		const auto base = lua_gettop(L);

		lua_getglobal(L, "string");
		lua_getfield(L, -1, "format");
		for (auto i = 1; i <= argc; ++i)
		{
			lua_pushvalue(L, i);
		}
		lua_call(L, argc, 1);

		std::string message = vm::to_string(L, -1);

		lua_settop(L, base);
		return message;
	}

	static int log_info(lua_State* L)
	{
		LOG_INFO("[{}] {}", bound_env(L).mod().mod_name(), formatted_message(L));
		return 0;
	}

	static int log_warn(lua_State* L)
	{
		LOG_WARN("[{}] {}", bound_env(L).mod().mod_name(), formatted_message(L));
		return 0;
	}

	static int log_error(lua_State* L)
	{
		LOG_ERROR("[{}] {}", bound_env(L).mod().mod_name(), formatted_message(L));
		return 0;
	}

	static int log_debug(lua_State* L)
	{
		LOG_DEBUG("[{}] {}", bound_env(L).mod().mod_name(), formatted_message(L));
		return 0;
	}

	static void push_log_table(ScriptEnv& env, lua_State* L)
	{
		lua_newtable(L);

		push_bound_function(env, L, "info", &log_info);
		lua_setfield(L, -2, "info");

		push_bound_function(env, L, "warn", &log_warn);
		lua_setfield(L, -2, "warn");

		push_bound_function(env, L, "error", &log_error);
		lua_setfield(L, -2, "error");

		push_bound_function(env, L, "debug", &log_debug);
		lua_setfield(L, -2, "debug");

		lua_setreadonly(L, -1, true);
	}

	static std::filesystem::path mod_relative(lua_State* L, const char* relative)
	{
		std::filesystem::path path(relative);
		if (path.is_absolute())
			return path;
		const auto& manifest = bound_env(L).mod().manifest;
		return manifest ? manifest->root / path : path;
	}

	static int assets_register_file(lua_State* L)
	{
		const auto id = assets::register_file(mod_relative(L, luaL_checkstring(L, 1)));
		if (!id)
			luaL_error(L, "%s", id.error().c_str());
		lua_pushstring(L, id->c_str());
		return 1;
	}

	static int assets_mesh_from_obj(lua_State* L)
	{
		const auto source = mod_relative(L, luaL_checkstring(L, 1));
		const std::string stem = lua_isstring(L, 2) ? lua_tostring(L, 2) : source.stem().string();
		const auto mesh = assets::mesh::load_obj(source);
		if (!mesh)
			luaL_error(L, "%s", mesh.error().c_str());

		const auto bytes = assets::mesh::encode_v2(*mesh);
		const auto id = assets::register_bytes(mod_relative(L, "cache") / (stem + ".mesh"), bytes.data(), bytes.size());
		if (!id)
			luaL_error(L, "%s", id.error().c_str());

		lua_pushstring(L, id->c_str());
		lua_pushinteger(L, static_cast<int>(mesh->vertices.size()));
		lua_pushinteger(L, static_cast<int>(mesh->faces.size()));
		return 3;
	}

	static int assets_is_registered(lua_State* L)
	{
		const auto factory = assets::temporary_id_factory();
		if (!factory)
			luaL_error(L, "%s", factory.error().c_str());
		lua_pushboolean(L, (*factory)->knows_temporary_id(RBX::ContentId(luaL_checkstring(L, 1))));
		return 1;
	}

	static void push_assets_table(ScriptEnv& env, lua_State* L)
	{
		lua_newtable(L);

		push_bound_function(env, L, "register_file", &assets_register_file);
		lua_setfield(L, -2, "register_file");

		push_bound_function(env, L, "mesh_from_obj", &assets_mesh_from_obj);
		lua_setfield(L, -2, "mesh_from_obj");

		push_bound_function(env, L, "is_registered", &assets_is_registered);
		lua_setfield(L, -2, "is_registered");

		lua_setreadonly(L, -1, true);
	}

	static int rml_on_unload(lua_State* L)
	{
		luaL_checktype(L, 1, LUA_TFUNCTION);

		auto& env = bound_env(L);
		env.add_unload_handler(anchor_in_env(env, L, 1));

		return 0;
	}

	static void push_mod_table(const ScriptEnv& env, lua_State* L)
	{
		const auto& manifest = env.mod().manifest;

		lua_newtable(L);

		lua_pushstring(L, manifest ? manifest->name.c_str() : "");
		lua_setfield(L, -2, "name");

		lua_pushstring(L, manifest ? manifest->version.c_str() : "");
		lua_setfield(L, -2, "version");

		lua_pushstring(L, manifest ? manifest->author.c_str() : "");
		lua_setfield(L, -2, "author");

		lua_pushstring(L, manifest ? manifest->description.c_str() : "");
		lua_setfield(L, -2, "description");

		const auto path = manifest ? manifest->root.generic_string() : std::string{};
		lua_pushstring(L, path.c_str());
		lua_setfield(L, -2, "path");

		lua_setreadonly(L, -1, true);
	}

	bool bind_rml(ScriptEnv& env, lua_State* L) noexcept
	{
		lua_newtable(L);

		push_log_table(env, L);
		lua_setfield(L, -2, "log");

		push_mod_table(env, L);
		lua_setfield(L, -2, "mod");

		push_assets_table(env, L);
		lua_setfield(L, -2, "assets");

		push_bound_function(env, L, "on_unload", &rml_on_unload);
		lua_setfield(L, -2, "on_unload");

		lua_setreadonly(L, -1, true);
		lua_setglobal(L, "rml");

		return true;
	}
}
