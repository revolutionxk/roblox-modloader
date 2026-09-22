#include "RobloxModLoader/luau/script_host.hpp"

#include "RobloxModLoader/luau/luau_bridge.hpp"
#include "RobloxModLoader/luau/modules/module_resolver.hpp"
#include "RobloxModLoader/luau/script_runtime.hpp"
#include "RobloxModLoader/luau/vm/chunk.hpp"
#include "RobloxModLoader/luau/vm/stack_guard.hpp"
#include "RobloxModLoader/luau/vm/thread_identity.hpp"
#include "RobloxModLoader/luau/vm/vm_api.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"
#include "RobloxModLoader/roblox/security/script_permissions.hpp"

RML_LOG_SCOPE("ScriptHost");

namespace rml::luau
{
	ScriptHost::ScriptHost(ScriptRuntime& runtime, const RBX::DataModelType type, lua_State* global_state) :
	    m_runtime(&runtime),
	    m_type(type),
	    m_global(global_state)
	{
	}

	ScriptHost::~ScriptHost()
	{
		shutdown();
	}

	bool ScriptHost::on_owner_thread() const noexcept
	{
		const auto owner = m_owner.load(std::memory_order_acquire);
		return owner == std::thread::id{} || owner == std::this_thread::get_id();
	}

	std::expected<ScriptEnv*, vm::VmError> ScriptHost::env_for(const ModManifestPtr& mod)
	{
		const auto key = mod ? mod->name : std::string{};

		if (const auto it = m_mod_envs.find(key); it != m_mod_envs.end())
		{
			return it->second.get();
		}

		auto thread = vm::Thread::spawn(m_global);
		if (!thread)
		{
			return std::unexpected(thread.error());
		}

		auto* raw = thread->get();
		luaL_sandboxthread(raw);

		vm::set_identity(raw, RBX::Security::Permissions::RobloxEngine, RBX::Security::FULL_CAPABILITIES);

		auto env = std::make_unique<ScriptEnv>(*this, std::move(*thread), m_runtime->environment_for(mod));

		vm::StackGuard guard(raw);
		if (!bind_globals(*env, raw))
		{
			RML_WARN("Some globals failed to bind for mod '{}'", key.empty() ? "<loader>" : key);
		}

		auto [it, _] = m_mod_envs.emplace(key, std::move(env));
		return it->second.get();
	}

	std::expected<void, vm::VmError> ScriptHost::post_script(const ModManifestPtr& mod, const ScriptAsset& asset)
	{
		if (!vm::api_ready())
		{
			vm::report_api_unavailable_once();
			return std::unexpected(vm::VmError::unavailable("the Luau C API is not resolved on this Studio build"));
		}

		auto source = read_source(asset.path);
		if (!source)
		{
			return std::unexpected(vm::VmError::internal(std::move(source.error())));
		}

		auto chunk_name = logical_name_for(asset.path, m_runtime->environment_for(mod));
		if (chunk_name.empty())
		{
			chunk_name = asset.chunk_name();
		}

		auto bytecode = BytecodeCache::compile(*source, chunk_name);
		if (!bytecode)
		{
			return std::unexpected(std::move(bytecode.error()));
		}

		m_dispatcher.post(RunChunk{
		    .chunk_name = std::move(chunk_name),
		    .bytecode = std::move(*bytecode),
		    .owner = mod,
		    .want_result = false,
		    .generation = mod ? generation_of(mod->name) : 0,
		});

		return {};
	}

	std::uint64_t ScriptHost::generation_of(const std::string& mod_name) const noexcept
	{
		const auto it = m_generations.find(mod_name);
		return it == m_generations.end() ? 0 : it->second;
	}

	void ScriptHost::pump(const Budget& budget) noexcept
	{
		m_owner.store(std::this_thread::get_id(), std::memory_order_release);

		prune_parked();

		if (const auto pending = m_dispatcher.pending_count(); pending > 0)
		{
			RML_INFO("Pumping {} queued item(s) for DataModel type {}", pending, static_cast<int>(m_type));
		}

		m_dispatcher.pump(*this, budget);
	}

	static std::expected<void, vm::VmError> push_value(lua_State* L, const Value& value, ScriptHost& host)
	{
		return std::visit(
		    [&]<typename H>(const H& held) -> std::expected<void, vm::VmError> {
			    using Held = std::decay_t<H>;

			    if constexpr (std::is_same_v<Held, std::monostate>)
			    {
				    lua_pushnil(L);
			    }
			    else if constexpr (std::is_same_v<Held, bool>)
			    {
				    lua_pushboolean(L, held ? 1 : 0);
			    }
			    else if constexpr (std::is_same_v<Held, double>)
			    {
				    lua_pushnumber(L, held);
			    }
			    else if constexpr (std::is_same_v<Held, std::string>)
			    {
				    lua_pushlstring(L, held.data(), held.size());
			    }
			    else if constexpr (std::is_same_v<Held, LuauRefHandle>)
			    {
				    auto* ref = host.lookup(held.id);
				    if (ref == nullptr)
				    {
					    return std::unexpected(vm::VmError::internal("the Luau handle passed in is no longer live"));
				    }
				    ref->push(L);
			    }
			    else
			    {
				    return std::unexpected(vm::VmError::unavailable("passing an engine Instance into Luau is not wired up yet"));
			    }

			    return {};
		    },
		    value);
	}

	static Value read_value(lua_State* L, const int index, ScriptHost& host)
	{
		switch (lua_type(L, index))
		{
		case LUA_TNIL: return Value{};
		case LUA_TBOOLEAN: return lua_toboolean(L, index) != 0;
		case LUA_TNUMBER: return lua_tonumberx(L, index, nullptr);
		case LUA_TSTRING:
		{
			std::size_t length = 0;
			const auto* text = lua_tolstring(L, index, &length);
			return std::string{text ? text : "", text ? length : 0};
		}
		default: break;
		}

		lua_pushvalue(L, index);
		auto ref = vm::Ref::take(L, -1, host.global_state());
		lua_pop(L, 1);

		return LuauRefHandle{host.retain(std::move(ref))};
	}

	static int report_script_error(lua_State* L)
	{
		std::size_t length = 0;
		const auto* raw = lua_tolstring(L, 1, &length);
		std::string message = raw ? std::string{raw, length} : "unknown Luau error";

		const auto traceback = vm::capture_traceback(L);
		const auto* label = lua_tolstring(L, lua_upvalueindex(1), nullptr);

		RML_ERROR("{} raised: {}{}{}", label ? label : "a mod script", message, traceback.empty() ? "" : "\n", traceback);

		if (!traceback.empty())
		{
			message.append("\n").append(traceback);
		}

		lua_pushlstring(L, message.data(), message.size());
		return 1;
	}

	static void push_error_handler(lua_State* L, const std::string& label)
	{
		lua_pushlstring(L, label.data(), label.size());
		lua_pushcclosure(L, &report_script_error, "rml_script_error", 1);
	}

	static bool push_xpcall(lua_State* L)
	{
		lua_getglobal(L, "xpcall");
		if (lua_isfunction(L, -1))
		{
			return true;
		}

		lua_pop(L, 1);
		return false;
	}

	// Takes the elevation over from the caller: the guard has to outlive the thread ref
	// (its restore touches the lua_State) and, when the chunk yields, the elevation
	// travels with the parked thread so the engine's resume still runs elevated.
	static WorkResult start_thread(ScriptHost& host, vm::Thread parked_thread, vm::ScopedIdentity parked_identity, const int nargs, const bool wrapped, const bool want_result, const std::string& label, std::string owner)
	{
		// Declared in this order so the elevation is handed back before the anchor that
		// keeps the coroutine out of the GC's reach goes away, on every path.
		vm::Thread thread = std::move(parked_thread);
		vm::ScopedIdentity identity = std::move(parked_identity);

		auto* co = thread.get();

		const auto outcome = vm::resume(co, nargs);
		if (!outcome)
		{
			return std::unexpected(outcome.error());
		}

		if (outcome->suspended)
		{
			if (const auto state = vm::task_state(co); state == RBX::Luau::TaskState::None)
			{
				RML_WARN("{} yielded but nothing in the engine owns the resume", label);
			}
			else
			{
				RML_DEBUG("{} yielded under task state {}", label, static_cast<int>(state));
			}

			host.park(std::move(thread), std::move(identity), std::move(owner), label);
			return Value{};
		}

		const auto produced = outcome->results;

		if (!wrapped)
		{
			return want_result && produced >= 1 ? read_value(co, 1, host) : Value{};
		}

		if (produced < 1 || lua_toboolean(co, 1) == 0)
		{
			if (!want_result)
			{
				return Value{};
			}

			std::size_t length = 0;
			const auto* raw = produced >= 2 ? lua_tolstring(co, 2, &length) : nullptr;
			return std::unexpected(vm::VmError{.kind = vm::VmError::Kind::Runtime, .message = raw ? std::string{raw, length} : "the script failed", .reported = true});
		}

		return want_result && produced >= 2 ? read_value(co, 2, host) : Value{};
	}

	static WorkResult run_chunk(ScriptHost& host, RunChunk& chunk)
	{
		if (chunk.owner && chunk.generation != host.generation_of(chunk.owner->name))
		{
			return Value{};
		}

		auto env = host.env_for(chunk.owner);
		if (!env)
		{
			return std::unexpected(env.error());
		}

		auto thread = vm::Thread::spawn((*env)->thread(), host.global_state());
		if (!thread)
		{
			return std::unexpected(thread.error());
		}

		auto* co = thread->get();

		// Studio gates settings() and friends on the identity context, not on the extra
		// space, and a fresh coroutine arrives with the engine's default identity. Elevate
		// the context for as long as this chunk runs, a yield included, and hand
		// it back afterwards: contexts are pooled, so a permanent write would follow the
		// object into the next thread.
		vm::ScopedIdentity identity{co, RBX::Security::Permissions::RobloxEngine, RBX::Security::FULL_CAPABILITIES};

		const auto label = std::format("script '{}'", chunk.chunk_name);
		const auto wrapped = push_xpcall(co);

		if (auto loaded = load_chunk_for(**env, co, chunk.chunk_name, chunk.bytecode,
		                                 RBX::Security::FULL_CAPABILITIES, (*env)->mod().node_for(chunk.chunk_name));
		    !loaded)
		{
			return std::unexpected(std::move(loaded.error()));
		}

		if (wrapped)
		{
			push_error_handler(co, label);
		}

		return start_thread(host,
		    std::move(*thread),
		    std::move(identity),
		    wrapped ? 2 : 0,
		    wrapped,
		    chunk.want_result,
		    label,
		    chunk.owner ? chunk.owner->name : std::string{});
	}

	static WorkResult call_ref(ScriptHost& host, const CallRef& call)
	{
		const auto* target = host.lookup(call.target);
		if (target == nullptr)
		{
			return std::unexpected(vm::VmError::internal("the Luau handle being called is no longer live"));
		}

		auto env = host.loader_env();
		if (!env)
		{
			return std::unexpected(env.error());
		}

		auto thread = vm::Thread::spawn((*env)->thread(), host.global_state());
		if (!thread)
		{
			return std::unexpected(thread.error());
		}

		auto* co = thread->get();

		// Same story as run_chunk: a callback invoked from .NET runs on a fresh coroutine
		// that arrives with the engine's default identity.
		vm::ScopedIdentity identity{co, RBX::Security::Permissions::RobloxEngine, RBX::Security::FULL_CAPABILITIES};

		const auto label = std::string{"a script callback"};
		const auto wrapped = push_xpcall(co);

		if (!target->push(co))
		{
			return std::unexpected(vm::VmError::internal("the Luau handle being called is no longer live"));
		}

		if (wrapped)
		{
			push_error_handler(co, label);
		}

		for (const auto& argument : call.args)
		{
			if (auto pushed = push_value(co, argument, host); !pushed)
			{
				return std::unexpected(std::move(pushed.error()));
			}
		}

		const auto argc = static_cast<int>(call.args.size());

		return start_thread(host, std::move(*thread), std::move(identity), (wrapped ? 2 : 0) + argc, wrapped, true, label, {});
	}

	static WorkResult index_ref(ScriptHost& host, const IndexRef& index)
	{
		const auto* target = host.lookup(index.target);
		if (target == nullptr)
		{
			return std::unexpected(vm::VmError::internal("the Luau handle being indexed is no longer live"));
		}

		auto env = host.loader_env();
		if (!env)
		{
			return std::unexpected(env.error());
		}

		auto* L = (*env)->thread();
		vm::StackGuard guard(L);

		target->push(L);
		lua_getfield(L, -1, index.key.c_str());

		return read_value(L, -1, host);
	}

	static WorkResult reload_requested(ScriptHost& host, const ReloadMod& request)
	{
		auto plan = host.runtime().plan_reload(request.mod_name, host.type());
		if (!plan)
		{
			return std::unexpected(vm::VmError::internal(std::format("cannot reload '{}': {}", request.mod_name, plan.error())));
		}

		if (auto reloaded = host.reload_mod(*plan); !reloaded)
		{
			return std::unexpected(std::move(reloaded.error()));
		}

		return Value{};
	}

	void ScriptHost::execute(Work& work, std::function<void(WorkResult)> settle) noexcept
	{
		auto result = std::visit(
		    [this]<typename H>(H& held) -> WorkResult {
			    using Held = std::decay_t<H>;

			    if constexpr (std::is_same_v<Held, RunChunk>)
			    {
				    return run_chunk(*this, held);
			    }
			    else if constexpr (std::is_same_v<Held, CallRef>)
			    {
				    return call_ref(*this, held);
			    }
			    else if constexpr (std::is_same_v<Held, IndexRef>)
			    {
				    return index_ref(*this, held);
			    }
			    else if constexpr (std::is_same_v<Held, ReloadMod>)
			    {
				    return reload_requested(*this, held);
			    }
			    else
			    {
				    release(held.target);
				    return Value{};
			    }
		    },
		    work);

		settle(std::move(result));
	}

	// The elevation travels with the thread: the engine resumes a parked coroutine long
	// after run_chunk returned, and it has to resume with the identity it yielded under.
	// Dropping the guard here (invalid thread) restores immediately, which is what an
	// unresumable thread wants.
	void ScriptHost::park(vm::Thread thread, vm::ScopedIdentity identity, std::string owner, std::string label)
	{
		if (!thread.valid())
		{
			return;
		}

		m_parked.push_back(ParkedThread{.thread = std::move(thread), .identity = std::move(identity), .owner = std::move(owner), .label = std::move(label)});
	}

	void ScriptHost::prune_parked() noexcept
	{
		if (m_parked.empty() || m_shutdown)
		{
			return;
		}

		std::erase_if(m_parked, [](const ParkedThread& parked) {
			return !vm::is_suspended(parked.thread);
		});
	}

	void ScriptHost::close_parked_of(const std::string& mod_name) noexcept
	{
		if (m_parked.empty())
		{
			return;
		}

		const auto env = loader_env();
		if (!env)
		{
			return;
		}

		auto* L = (*env)->thread();

		std::erase_if(m_parked, [&](const ParkedThread& parked) {
			if (parked.owner != mod_name)
			{
				return false;
			}

			if (!vm::close_thread(L, parked.thread))
			{
				RML_WARN("{} of mod '{}' stayed suspended through the reload", parked.label, mod_name);
				return false;
			}

			return true;
		});
	}

	RefId ScriptHost::retain(vm::Ref ref, std::string owner)
	{
		const auto id = m_runtime->next_ref_id();
		m_refs.emplace(id, RefEntry{.ref = std::move(ref), .owner = std::move(owner)});
		m_runtime->map_ref(id, this);
		return id;
	}

	vm::Ref* ScriptHost::lookup(const RefId id) noexcept
	{
		const auto it = m_refs.find(id);
		return it == m_refs.end() ? nullptr : &it->second.ref;
	}

	void ScriptHost::release(const RefId id) noexcept
	{
		m_refs.erase(id);
		m_runtime->unmap_ref(id);
	}

	void ScriptHost::run_unload_handlers(ScriptEnv& env)
	{
		auto handlers = env.take_unload_handlers();
		if (handlers.empty())
		{
			return;
		}

		auto* L = env.thread();

		for (auto& handler : std::views::reverse(handlers))
		{
			vm::StackGuard guard(L);

			if (!handler.push(L))
			{
				continue;
			}

			if (const auto called = vm::protected_call(L, 0, 0); !called)
			{
				RML_ERROR("An unload handler of mod '{}' failed: {}", env.mod().mod_name(), called.error().describe());
			}
		}
	}

	void ScriptHost::release_mod_state(const std::string& mod_name, ScriptEnv& env)
	{
		close_parked_of(mod_name);

		auto* L = env.thread();

		for (auto& [target, record] : m_closures.take_hooks_of(mod_name))
		{
			if (!restore_closure(L, record))
			{
				RML_WARN("Could not restore a function hooked by mod '{}'", mod_name);
			}
		}

		m_closures.release_owner(mod_name);

		std::vector<RefId> dropped;
		for (const auto& [id, entry] : m_refs)
		{
			if (entry.owner == mod_name)
			{
				dropped.push_back(id);
			}
		}

		for (const auto id : dropped)
		{
			release(id);
		}

		if (!dropped.empty())
		{
			m_runtime->bridge().drop_script_callbacks(m_type, dropped);
		}
	}

	std::expected<void, vm::VmError> ScriptHost::reload_mod(const ModReloadPlan& plan)
	{
		if (!plan.manifest)
		{
			return std::unexpected(vm::VmError::internal("reload asked for a mod with no manifest"));
		}

		if (!vm::api_ready())
		{
			vm::report_api_unavailable_once();
			return std::unexpected(vm::VmError::unavailable("the Luau C API is not resolved on this Studio build"));
		}

		const auto& mod_name = plan.manifest->name;

		++m_generations[mod_name];

		if (const auto it = m_mod_envs.find(mod_name); it != m_mod_envs.end())
		{
			const auto retired = std::move(it->second);
			m_mod_envs.erase(it);

			run_unload_handlers(*retired);
			release_mod_state(mod_name, *retired);

			m_expired_tokens.push_back(retired->token());
		}

		const auto scripts_root = plan.manifest->scripts_root();

		auto dropped = m_bytecode.invalidate_under(scripts_root);
		for (const auto& env : m_mod_envs | std::views::values)
		{
			if (env)
			{
				dropped += env->modules().invalidate_under(scripts_root);
			}
		}

		std::size_t posted = 0;
		for (const auto& asset : plan.scripts)
		{
			if (auto queued = post_script(plan.manifest, asset); !queued)
			{
				RML_ERROR("Could not queue '{}' for mod '{}': {}", asset.chunk_name(), mod_name, queued.error().message);
				continue;
			}

			++posted;
		}

		RML_INFO("Reloaded mod '{}' on DataModel type {}: {} module(s) dropped, {} script(s) queued", mod_name, static_cast<int>(m_type), dropped, posted);

		return {};
	}

	void ScriptHost::shutdown() noexcept
	{
		if (m_shutdown)
		{
			return;
		}
		m_shutdown = true;

		m_dispatcher.drain_cancelled(vm::VmError::unavailable("the DataModel this script host served went away"));

		m_runtime->bridge().drop_script_listeners(m_type);
		m_runtime->unmap_refs_of(this);

		for (auto& entry : m_refs | std::views::values)
		{
			entry.ref.release();
		}
		m_refs.clear();

		for (auto& parked : m_parked)
		{
			// Un-elevate the pooled context while the thread is still anchored: releasing
			// the anchor first would leave the restore reading a collectable state.
			parked.identity.restore();
			parked.thread.release();
		}
		m_parked.clear();

		m_bytecode.clear();
		m_closures.release();

		for (auto& env : m_mod_envs | std::views::values)
		{
			if (env)
			{
				env->release();
			}
		}

		m_mod_envs.clear();
		m_expired_tokens.clear();
		m_generations.clear();
	}
}
