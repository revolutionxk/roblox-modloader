#pragma once

#include "RobloxModLoader/luau/dispatch/dispatcher.hpp"
#include "RobloxModLoader/luau/env/binding.hpp"
#include "RobloxModLoader/luau/env/closure_registry.hpp"
#include "RobloxModLoader/luau/modules/bytecode_cache.hpp"
#include "RobloxModLoader/luau/script/script_asset.hpp"
#include "RobloxModLoader/luau/vm/lua_thread.hpp"
#include "RobloxModLoader/luau/vm/thread_identity.hpp"
#include <functional>
#include <unordered_map>

namespace RBX
{
	enum class DataModelType : std::int32_t;
}

namespace rml::luau
{
	class ScriptRuntime;

	struct ModReloadPlan
	{
		ModManifestPtr manifest;
		std::vector<ScriptAsset> scripts;
	};

	class RML_EXPORT ScriptHost final
	{
	public:
		ScriptHost(ScriptRuntime& runtime, RBX::DataModelType type, lua_State* global_state);
		~ScriptHost();

		ScriptHost(const ScriptHost&) = delete;
		ScriptHost& operator=(const ScriptHost&) = delete;
		ScriptHost(ScriptHost&&) = delete;
		ScriptHost& operator=(ScriptHost&&) = delete;

		[[nodiscard]] RBX::DataModelType type() const noexcept { return m_type; }
		[[nodiscard]] lua_State* global_state() const noexcept { return m_global; }
		[[nodiscard]] ScriptRuntime& runtime() const noexcept { return *m_runtime; }

		[[nodiscard]] Dispatcher& dispatcher() noexcept { return m_dispatcher; }
		[[nodiscard]] BytecodeCache& bytecode() noexcept { return m_bytecode; }
		[[nodiscard]] ClosureRegistry& closures() noexcept { return m_closures; }

		void pump(const Budget& budget) noexcept;

		[[nodiscard]] bool has_pending() const noexcept { return m_dispatcher.has_pending(); }

		std::expected<void, vm::VmError> post_script(const ModManifestPtr& mod, const ScriptAsset& asset);

		std::expected<ScriptEnv*, vm::VmError> env_for(const ModManifestPtr& mod);

		std::expected<ScriptEnv*, vm::VmError> loader_env() { return env_for(nullptr); }

		void park(vm::Thread thread, vm::ScopedIdentity identity, std::string owner, std::string label);

		[[nodiscard]] RefId retain(vm::Ref ref, std::string owner = {});
		[[nodiscard]] vm::Ref* lookup(RefId id) noexcept;
		void release(RefId id) noexcept;

		std::expected<void, vm::VmError> reload_mod(const ModReloadPlan& plan);

		[[nodiscard]] std::uint64_t generation_of(const std::string& mod_name) const noexcept;

		void shutdown() noexcept;

	private:
		struct RefEntry
		{
			vm::Ref ref;
			std::string owner;
		};

		struct ParkedThread
		{
			vm::Thread thread;
			// Destroyed before the thread anchor above, so the elevation is handed back
			// while the coroutine is still anchored.
			vm::ScopedIdentity identity;
			std::string owner;
			std::string label;
		};

		[[nodiscard]] bool on_owner_thread() const noexcept;

		void prune_parked() noexcept;
		void close_parked_of(const std::string& mod_name) noexcept;

		void execute(Work& work, std::function<void(WorkResult)> settle) noexcept;

		void run_unload_handlers(ScriptEnv& env);
		void release_mod_state(const std::string& mod_name, ScriptEnv& env);

		ScriptRuntime* m_runtime;
		RBX::DataModelType m_type;
		lua_State* m_global;

		Dispatcher m_dispatcher;
		BytecodeCache m_bytecode;
		ClosureRegistry m_closures;

		std::unordered_map<std::string, std::unique_ptr<ScriptEnv>> m_mod_envs;
		std::unordered_map<RefId, RefEntry> m_refs;
		std::unordered_map<std::string, std::uint64_t> m_generations;
		std::vector<EnvTokenPtr> m_expired_tokens;
		std::vector<ParkedThread> m_parked;

		std::atomic<std::thread::id> m_owner{};
		bool m_shutdown{false};

		friend class Dispatcher;
	};
}
