#include "RobloxModLoader/luau/modules/module_registry.hpp"

#include "RobloxModLoader/luau/env/binding.hpp"
#include "RobloxModLoader/luau/vm/chunk.hpp"
#include "RobloxModLoader/luau/vm/stack_guard.hpp"
#include "RobloxModLoader/luau/vm/thread_identity.hpp"
#include "RobloxModLoader/util/filesystem.hpp"
#include "RobloxModLoader/util/string.hpp"

RML_LOG_SCOPE("Modules");

namespace rml::luau
{
	struct LoadingScope final
	{
		LoadingScope(std::vector<ResolvedModule>& loading, const ResolvedModule& module)
			: m_loading(loading)
		{
			m_loading.push_back(module);
		}

		~LoadingScope() { m_loading.pop_back(); }

		LoadingScope(const LoadingScope&) = delete;
		LoadingScope& operator=(const LoadingScope&) = delete;
		LoadingScope(LoadingScope&&) = delete;
		LoadingScope& operator=(LoadingScope&&) = delete;

	private:
		std::vector<ResolvedModule>& m_loading;
	};

	std::expected<void, vm::VmError> ModuleRegistry::require(
	    lua_State* L, const ResolvedModule& module, ScriptEnv& env)
	{
		if (const auto cached = m_loaded.find(module.id); cached != m_loaded.end())
		{
			vm::StackGuard guard(L);

			if (cached->second.returned_nil)
			{
				lua_pushnil(L);
				guard.release();
				return {};
			}

			if (!cached->second.value.push(L))
			{
				return std::unexpected(vm::VmError::internal(
				    std::format("module '{}' lost its cached value", module.logical)));
			}

			guard.release();
			return {};
		}

		const auto repeated = std::ranges::find(m_loading, module.id, &ResolvedModule::id);
		if (repeated != m_loading.end())
		{
			return std::unexpected(vm::VmError::internal(describe_cycle(module)));
		}

		return load(L, module, env);
	}

	std::expected<void, vm::VmError> ModuleRegistry::load(
	    lua_State* L, const ResolvedModule& module, ScriptEnv& env)
	{
		const LoadingScope scope(m_loading, module);

		const auto bytecode = m_bytecode->acquire(module.id);
		if (!bytecode)
		{
			return std::unexpected(bytecode.error());
		}

		vm::StackGuard guard(L);

		if (auto loaded = load_chunk_for(env, L, module.logical, *bytecode, RBX::Security::FULL_CAPABILITIES,
		                                 env.mod().node_for(module.logical));
		    !loaded)
		{
			return std::unexpected(std::move(loaded.error()));
		}

		vm::set_identity(L, RBX::Security::Permissions::RobloxEngine, RBX::Security::FULL_CAPABILITIES, false);

		const auto called = vm::protected_call(L, 0, LUA_MULTRET);
		if (!called)
		{
			return std::unexpected(called.error());
		}

		if (*called != 1)
		{
			return std::unexpected(vm::VmError::internal(
			    std::format("module '{}' returned {} values, a module must return exactly one value",
			                module.logical, *called)));
		}

		if (lua_isnil(L, -1))
		{
			m_loaded.insert_or_assign(module.id, LoadedModule{.returned_nil = true});

			guard.release();

			RML_DEBUG("Loaded module '{}' for '{}' (it returns nil)", module.logical, env.mod().mod_name());
			return {};
		}

		auto ref = vm::Ref::take(L, -1, m_anchor);
		if (!ref.valid())
		{
			return std::unexpected(
			    vm::VmError::internal(std::format("module '{}' could not be anchored", module.logical)));
		}

		m_loaded.insert_or_assign(module.id, LoadedModule{.value = std::move(ref)});

		guard.release();

		RML_DEBUG("Loaded module '{}' for '{}'", module.logical, env.mod().mod_name());
		return {};
	}

	std::string ModuleRegistry::describe_cycle(const ResolvedModule& repeated) const
	{
		auto chain = m_loading | std::views::transform(&ResolvedModule::logical) | std::ranges::to<std::vector<std::string>>();
		chain.push_back(repeated.logical);
		return "module cycle: " + utils::join(chain, " -> ");
	}

	void ModuleRegistry::invalidate(const ModuleId& id)
	{
		m_loaded.erase(id);
	}

	std::size_t ModuleRegistry::invalidate_under(const std::filesystem::path& root)
	{
		return utils::erase_under(m_loaded, root, &ModuleId::string);
	}

	void ModuleRegistry::clear() noexcept
	{
		m_loaded.clear();
		m_loading.clear();
	}

	void ModuleRegistry::release() noexcept
	{
		for (auto& entry : m_loaded | std::views::values)
		{
			entry.value.release();
		}

		clear();
	}
}
