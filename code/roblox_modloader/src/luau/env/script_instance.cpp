#include "RobloxModLoader/luau/env/script_instance.hpp"

#include "RobloxModLoader/luau/env/binding.hpp"
#include "RobloxModLoader/luau/script_host.hpp"
#include "RobloxModLoader/luau/vm/chunk.hpp"
#include "RobloxModLoader/luau/vm/stack.hpp"
#include "RobloxModLoader/luau/vm/stack_guard.hpp"

#include <cstring>

RML_LOG_SCOPE("ScriptInstance");

namespace rml::luau
{
	struct NodeTag
	{
		EnvToken* token{nullptr};
		const ScriptNode* node{nullptr};
	};

	const ScriptNode* to_script_node(lua_State* L, const int index, const ScriptEnv* env) noexcept
	{
		const auto subject = vm::abs_index(L, index);

		if (lua_type(L, subject) != LUA_TTABLE || !lua_getmetatable(L, subject))
		{
			return nullptr;
		}

		lua_getfield(L, -1, "__node");

		std::size_t length = 0;
		const auto* bytes = lua_tolstring(L, -1, &length);

		NodeTag tag{};
		const auto decoded = bytes != nullptr && length == sizeof(tag);
		if (decoded)
		{
			std::memcpy(&tag, bytes, sizeof(tag));
		}

		lua_pop(L, 2);

		if (!decoded || tag.token == nullptr || tag.token->env == nullptr)
		{
			return nullptr;
		}

		return env != nullptr && tag.token->env != env ? nullptr : tag.node;
	}

	static const ScriptNode& checked_node(lua_State* L, const ScriptEnv& env)
	{
		if (const auto* node = to_script_node(L, 1, &env))
		{
			return *node;
		}

		luaL_error(L, "expected a script tree node, this one belongs to a mod that was unloaded");
	}

	static int node_get_children(lua_State* L)
	{
		auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		lua_createtable(L, static_cast<int>(node.children.size()), 0);

		int slot = 0;
		for (const auto* child : node.children)
		{
			env.nodes().push(env, L, child);
			lua_rawseti(L, -2, ++slot);
		}

		return 1;
	}

	static void collect_descendants(NodeCache& cache, ScriptEnv& env, lua_State* L, const ScriptNode& node, int& slot)
	{
		for (const auto* child : node.children)
		{
			cache.push(env, L, child);
			lua_rawseti(L, -2, ++slot);
			collect_descendants(cache, env, L, *child, slot);
		}
	}

	static int node_get_descendants(lua_State* L)
	{
		auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		lua_createtable(L, 0, 0);

		int slot = 0;
		collect_descendants(env.nodes(), env, L, node, slot);

		return 1;
	}

	static int node_find_first_child(lua_State* L)
	{
		auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		std::size_t length = 0;
		const auto* name = luaL_checklstring(L, 2, &length);
		const std::string_view wanted{name, length};

		const auto* found = luaL_optboolean(L, 3, 0) != 0 ? node.descendant(wanted) : node.child(wanted);

		env.nodes().push(env, L, found);
		return 1;
	}

	static int node_find_first_ancestor(lua_State* L)
	{
		auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		std::size_t length = 0;
		const auto* name = luaL_checklstring(L, 2, &length);

		env.nodes().push(env, L, node.ancestor(std::string_view{name, length}));
		return 1;
	}

	static int node_wait_for_child(lua_State* L)
	{
		auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		std::size_t length = 0;
		const auto* name = luaL_checklstring(L, 2, &length);
		const std::string_view wanted{name, length};

		if (const auto* found = node.child(wanted))
		{
			env.nodes().push(env, L, found);
			return 1;
		}

		if (lua_isnoneornil(L, 3))
		{
			luaL_error(L, "infinite yield possible on %s:WaitForChild(\"%s\"), the loader's script tree never gains "
			              "children while a script runs",
			           node.full_name().c_str(), name);
		}

		lua_pushnil(L);
		return 1;
	}

	static int node_get_full_name(lua_State* L)
	{
		const auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		const auto rendered = node.full_name();
		vm::push_string(L, rendered);

		return 1;
	}

	static int node_is_a(lua_State* L)
	{
		const auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		std::size_t length = 0;
		const auto* queried = luaL_checklstring(L, 2, &length);

		lua_pushboolean(L, class_is_a(node.klass, std::string_view{queried, length}) ? 1 : 0);
		return 1;
	}

	static int node_is_descendant_of(lua_State* L)
	{
		const auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		const auto* other = to_script_node(L, 2, &env);
		lua_pushboolean(L, other != nullptr && node.descends_from(other) ? 1 : 0);

		return 1;
	}

	struct NodeMethod
	{
		std::string_view name;
		lua_CFunction fn;
	};

	static constexpr std::array kNodeMethods{
	    NodeMethod{"GetChildren", &node_get_children},
	    NodeMethod{"GetDescendants", &node_get_descendants},
	    NodeMethod{"FindFirstChild", &node_find_first_child},
	    NodeMethod{"FindFirstAncestor", &node_find_first_ancestor},
	    NodeMethod{"WaitForChild", &node_wait_for_child},
	    NodeMethod{"GetFullName", &node_get_full_name},
	    NodeMethod{"IsA", &node_is_a},
	    NodeMethod{"IsDescendantOf", &node_is_descendant_of},
	};

	static int node_index(lua_State* L)
	{
		auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		std::size_t length = 0;
		const auto* key = luaL_checklstring(L, 2, &length);
		const std::string_view wanted{key, length};

		if (wanted == "Name")
		{
			vm::push_string(L, node.name);
			return 1;
		}

		if (wanted == "ClassName")
		{
			vm::push_string(L, node.class_name());
			return 1;
		}

		if (wanted == "Parent")
		{
			env.nodes().push(env, L, node.parent);
			return 1;
		}

		lua_pushvalue(L, lua_upvalueindex(2));
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);

		if (!lua_isnil(L, -1))
		{
			return 1;
		}

		lua_pop(L, 2);

		if (const auto* child = node.child(wanted))
		{
			env.nodes().push(env, L, child);
			return 1;
		}

		luaL_error(L, "%s is not a valid member of %s \"%s\"", key, std::string{node.class_name()}.c_str(),
		           node.full_name().c_str());
	}

	static int node_newindex(lua_State* L)
	{
		const auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		const auto* key = luaL_checklstring(L, 2, nullptr);

		luaL_error(L, "cannot set %s on %s \"%s\", the loader's script tree mirrors disk and is read-only", key,
		           std::string{node.class_name()}.c_str(), node.full_name().c_str());
	}

	static int node_tostring(lua_State* L)
	{
		const auto& env = bound_env(L);
		const auto& node = checked_node(L, env);

		vm::push_string(L, node.name);
		return 1;
	}

	bool NodeCache::ensure_shared(ScriptEnv& env, lua_State* L)
	{
		if (m_index.valid())
		{
			return true;
		}

		auto* anchor = env.host().global_state();
		vm::StackGuard guard(L);

		lua_createtable(L, 0, static_cast<int>(kNodeMethods.size()));
		for (const auto& [name, fn] : kNodeMethods)
		{
			push_bound_function(env, L, name.data(), fn);
			lua_setfield(L, -2, name.data());
		}
		lua_setreadonly(L, -1, true);

		m_methods = vm::Ref::take(L, -1, anchor);
		if (!m_methods.valid())
		{
			return false;
		}

		push_bound_function(env, L, "__index", &node_index, 1);
		m_index = vm::Ref::take(L, -1, anchor);

		push_bound_function(env, L, "__newindex", &node_newindex);
		m_newindex = vm::Ref::take(L, -1, anchor);

		push_bound_function(env, L, "__tostring", &node_tostring);
		m_tostring = vm::Ref::take(L, -1, anchor);

		return m_index.valid() && m_newindex.valid() && m_tostring.valid();
	}

	void NodeCache::push_metatable(const ScriptEnv& env, lua_State* L, const ScriptNode* node) const
	{
		lua_createtable(L, 0, 5);

		m_index.push(L);
		lua_setfield(L, -2, "__index");

		m_newindex.push(L);
		lua_setfield(L, -2, "__newindex");

		m_tostring.push(L);
		lua_setfield(L, -2, "__tostring");

		lua_pushstring(L, "The metatable is locked");
		lua_setfield(L, -2, "__metatable");

		const NodeTag tag{.token = env.token().get(), .node = node};
		lua_pushlstring(L, reinterpret_cast<const char*>(&tag), sizeof(tag));
		lua_setfield(L, -2, "__node");

		lua_setreadonly(L, -1, true);
	}

	bool NodeCache::push(ScriptEnv& env, lua_State* L, const ScriptNode* node)
	{
		if (node == nullptr)
		{
			lua_pushnil(L);
			return false;
		}

		if (const auto cached = m_nodes.find(node); cached != m_nodes.end())
		{
			if (cached->second.push(L))
			{
				return true;
			}

			lua_pop(L, 1);
			m_nodes.erase(cached);
		}

		if (!ensure_shared(env, L))
		{
			RML_ERROR("Could not build the script node metatable for mod '{}'", env.mod().mod_name());
			lua_pushnil(L);
			return false;
		}

		lua_createtable(L, 0, 0);
		push_metatable(env, L, node);
		lua_setmetatable(L, -2);
		lua_setreadonly(L, -1, true);

		m_nodes.emplace(node, vm::Ref::take(L, -1, env.host().global_state()));
		return true;
	}

	void NodeCache::release() noexcept
	{
		for (auto& ref : m_nodes | std::views::values)
		{
			ref.release();
		}

		m_nodes.clear();
		m_methods.release();
		m_index.release();
		m_newindex.release();
		m_tostring.release();
	}

	std::expected<void, vm::VmError> load_chunk_for(ScriptEnv& env, lua_State* L, const std::string_view chunk_name,
	                                                const std::span<const std::byte> bytecode,
	                                                const std::uint64_t capabilities, const ScriptNode* node)
	{
		const auto slot = lua_gettop(L) + 1;

		lua_createtable(L, 0, 2);

		if (node != nullptr)
		{
			if (env.nodes().push(env, L, node))
			{
				lua_setfield(L, -2, "script");
			}
			else
			{
				lua_pop(L, 1);
			}
		}

		if (env.shared_globals().push(L))
		{
			lua_setfield(L, -2, "_G");
		}
		else
		{
			lua_pop(L, 1);
		}

		lua_createtable(L, 0, 1);
		if (auto* owner = env.thread(); owner != nullptr && owner != L)
		{
			vm::StackGuard guard(owner);
			lua_pushvalue(owner, LUA_GLOBALSINDEX);
			const auto globals = vm::Ref::take(owner, -1);
			if (!globals.push(L))
			{
				lua_pushvalue(L, LUA_GLOBALSINDEX);
			}
		}
		else
		{
			lua_pushvalue(L, LUA_GLOBALSINDEX);
		}
		lua_setfield(L, -2, "__index");
		lua_setreadonly(L, -1, true);
		lua_setmetatable(L, -2);

		auto loaded = vm::load_chunk(L, chunk_name, bytecode, capabilities, slot);
		if (!loaded)
		{
			return loaded;
		}

		lua_insert(L, slot);
		lua_settop(L, slot);

		return {};
	}
}
