#include "RobloxModLoader/luau/script_runtime.hpp"

#include "RobloxModLoader/luau/generated/layout_access.hpp"
#include "RobloxModLoader/luau/luau_bridge.hpp"
#include "RobloxModLoader/luau/vm/vm_api.hpp"
#include "lstate.h"
#include "luau/script/script_watcher.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"

RML_LOG_SCOPE("ScriptRuntime");

namespace rml::luau
{
	static ScriptRuntime* g_runtime = nullptr;

	ScriptRuntime* script_runtime() noexcept { return g_runtime; }

	void set_script_runtime(ScriptRuntime* runtime) noexcept { g_runtime = runtime; }

	ScriptRuntime::ScriptRuntime()
		: m_bridge(std::make_unique<Bridge>(*this))
	{
	}

	ScriptRuntime::~ScriptRuntime()
	{
		shutdown();
	}

	std::expected<void, std::string> ScriptRuntime::initialize(const std::filesystem::path& mods_directory)
	{
		m_mods_directory = mods_directory;
		m_loader_env = ModEnvironment{.manifest = nullptr, .rml_libraries = default_rml_libraries_root()};
		m_loader_env.libraries = build_script_tree(m_loader_env.rml_libraries, "rml", "rml", {});

		std::size_t catalogued = 0;
		{
			std::unique_lock lock(m_catalog_mutex);

			if (auto scanned = m_catalog.scan(mods_directory); !scanned)
			{
				return std::unexpected(std::move(scanned.error()));
			}

			catalogued = m_catalog.mods().size();
		}

		RML_INFO("Catalogued script mods: {}", catalogued);
		return {};
	}

	void ScriptRuntime::shutdown() noexcept
	{
		{
			std::lock_guard watcher_lock(m_watcher_mutex);
			if (m_watcher)
			{
				m_watcher->stop();
				m_watcher.reset();
			}
		}

		std::unique_lock lock(m_hosts_mutex);

		for (auto& host : m_hosts | std::views::values)
		{
			if (host)
			{
				host->shutdown();
			}
		}

		m_hosts.clear();
		m_by_global_state.clear();

		std::unique_lock catalog_lock(m_catalog_mutex);
		m_catalog.clear();
	}

	ModEnvironment ScriptRuntime::environment_for(const ModManifestPtr& mod) const
	{
		return ModEnvironment{
		    .manifest = mod, .rml_libraries = m_loader_env.rml_libraries, .libraries = m_loader_env.libraries};
	}

	void ScriptRuntime::bind_host(const RBX::DataModelType type, lua_State* global_state)
	{
		if (!global_state || !vm::api_ready())
		{
			vm::report_api_unavailable_once();
			return;
		}

		std::unique_lock lock(m_hosts_mutex);

		if (const auto it = m_hosts.find(type); it != m_hosts.end())
		{
			if (it->second->global_state() == global_state)
			{
				return;
			}

			it->second->shutdown();
			std::erase_if(m_by_global_state, [&](const auto& entry) { return entry.second == it->second.get(); });
			m_hosts.erase(it);
		}

		auto host = std::make_unique<ScriptHost>(*this, type, global_state);
		m_by_global_state[access::state(global_state)->global] = host.get();
		m_hosts[type] = std::move(host);

		RML_INFO("Bound a script host to DataModel type {}", std::to_underlying(type));
	}

	void ScriptRuntime::unbind_host(const RBX::DataModelType type)
	{
		std::unique_ptr<ScriptHost> retired;

		{
			std::unique_lock lock(m_hosts_mutex);

			const auto it = m_hosts.find(type);
			if (it == m_hosts.end())
			{
				return;
			}

			std::erase_if(m_by_global_state, [&](const auto& entry) { return entry.second == it->second.get(); });
			retired = std::move(it->second);
			m_hosts.erase(it);
		}

		retired->shutdown();
		RML_INFO("Unbound the script host for DataModel type {}", std::to_underlying(type));
	}

	ScriptHost* ScriptRuntime::host(const RBX::DataModelType type) noexcept
	{
		std::shared_lock lock(m_hosts_mutex);
		const auto it = m_hosts.find(type);
		return it == m_hosts.end() ? nullptr : it->second.get();
	}

	ScriptHost* ScriptRuntime::host_for(lua_State* thread) noexcept
	{
		if (!thread)
		{
			return nullptr;
		}

		std::shared_lock lock(m_hosts_mutex);
		const auto it = m_by_global_state.find(access::state(thread)->global);
		return it == m_by_global_state.end() ? nullptr : it->second;
	}

	ScriptHost* ScriptRuntime::host_for_ref(const RefId id) noexcept
	{
		std::shared_lock lock(m_refs_mutex);
		const auto it = m_ref_owners.find(id);
		return it == m_ref_owners.end() ? nullptr : it->second;
	}

	RefId ScriptRuntime::next_ref_id() noexcept { return m_next_ref.fetch_add(1, std::memory_order_relaxed); }

	void ScriptRuntime::map_ref(const RefId id, ScriptHost* owner)
	{
		std::unique_lock lock(m_refs_mutex);
		m_ref_owners[id] = owner;
	}

	void ScriptRuntime::unmap_ref(const RefId id) noexcept
	{
		std::unique_lock lock(m_refs_mutex);
		m_ref_owners.erase(id);
	}

	void ScriptRuntime::unmap_refs_of(const ScriptHost* owner) noexcept
	{
		std::unique_lock lock(m_refs_mutex);
		std::erase_if(m_ref_owners, [owner](const auto& entry) { return entry.second == owner; });
	}

	void ScriptRuntime::activate(const RBX::DataModelType type)
	{
		auto* target = host(type);
		if (!target)
		{
			RML_WARN("No script host is bound to DataModel type {}", std::to_underlying(type));
			return;
		}

		std::size_t posted = 0;

		{
			std::shared_lock lock(m_catalog_mutex);

			for (const auto& mod : m_catalog.mods())
			{
				for (const auto& asset : mod.for_context(type))
				{
					if (auto queued = target->post_script(mod.manifest, asset); !queued)
					{
						RML_ERROR("Could not queue '{}' for mod '{}': {}", asset.path.filename().string(),
						          mod.manifest->name, queued.error().message);
						continue;
					}

					++posted;
				}
			}
		}

		RML_INFO("Queued {} scripts for DataModel type {}", posted, std::to_underlying(type));
	}

	std::expected<ModReloadPlan, std::string> ScriptRuntime::plan_reload(const std::string& mod_name,
	                                                                     const RBX::DataModelType type) const
	{
		std::shared_lock lock(m_catalog_mutex);

		const auto* mod = m_catalog.find(mod_name);
		if (mod == nullptr)
		{
			return std::unexpected(std::format("unknown mod: '{}'", mod_name));
		}

		const auto scripts = mod->for_context(type);

		return ModReloadPlan{
		    .manifest = mod->manifest,
		    .scripts = std::vector<ScriptAsset>{scripts.begin(), scripts.end()},
		};
	}

	std::vector<std::pair<std::string, std::filesystem::path>> ScriptRuntime::script_roots() const
	{
		std::shared_lock lock(m_catalog_mutex);

		std::vector<std::pair<std::string, std::filesystem::path>> roots;
		roots.reserve(m_catalog.mods().size());

		for (const auto& mod : m_catalog.mods())
		{
			if (mod.manifest)
			{
				roots.emplace_back(mod.manifest->name, mod.manifest->scripts_root());
			}
		}

		return roots;
	}

	std::expected<void, std::string> ScriptRuntime::reload(const std::string_view mod_name)
	{
		{
			std::unique_lock lock(m_catalog_mutex);

			if (auto rescanned = m_catalog.rescan(mod_name); !rescanned)
			{
				return std::unexpected(std::move(rescanned.error()));
			}
		}

		std::size_t notified = 0;
		{
			std::shared_lock lock(m_hosts_mutex);

			for (auto& target : m_hosts | std::views::values)
			{
				if (!target)
				{
					continue;
				}

				target->dispatcher().post(ReloadMod{.mod_name = std::string{mod_name}});
				++notified;
			}
		}

		if (notified == 0)
		{
			RML_INFO("Rescanned mod '{}', no script host is bound yet", mod_name);
			return {};
		}

		RML_INFO("Queued a reload of mod '{}' on {} host(s)", mod_name, notified);
		return {};
	}

	std::size_t ScriptRuntime::reload_all()
	{
		std::vector<std::string> names;
		{
			std::shared_lock lock(m_catalog_mutex);

			names.reserve(m_catalog.mods().size());
			for (const auto& mod : m_catalog.mods())
			{
				if (mod.manifest)
				{
					names.push_back(mod.manifest->name);
				}
			}
		}

		std::size_t reloaded = 0;
		for (const auto& name : names)
		{
			if (auto queued = reload(name); !queued)
			{
				RML_ERROR("Could not reload '{}': {}", name, queued.error());
				continue;
			}

			++reloaded;
		}

		return reloaded;
	}

	void ScriptRuntime::set_hot_reload(const bool enabled)
	{
		std::lock_guard lock(m_watcher_mutex);

		if (!enabled)
		{
			if (m_watcher)
			{
				m_watcher->stop();
				m_watcher.reset();
			}

			return;
		}

		if (m_watcher)
		{
			return;
		}

		m_watcher = std::make_unique<ScriptWatcher>(*this);
		m_watcher->start();
	}

	bool ScriptRuntime::hot_reload_enabled() const noexcept
	{
		std::lock_guard lock(m_watcher_mutex);
		return m_watcher != nullptr;
	}

	static std::expected<RunChunk, vm::VmError> build_chunk(const std::string_view source,
	                                                        const std::string_view chunk_name, const bool want_result)
	{
		auto bytecode = BytecodeCache::compile(source, chunk_name);
		if (!bytecode)
		{
			return std::unexpected(std::move(bytecode.error()));
		}

		return RunChunk{
		    .chunk_name = std::string{chunk_name},
		    .bytecode = std::move(*bytecode),
		    .owner = nullptr,
		    .want_result = want_result,
		};
	}

	static std::expected<ScriptHost*, vm::VmError> host_or_error(const RBX::DataModelType context)
	{
		auto* runtime = script_runtime();
		if (!runtime)
		{
			return std::unexpected(vm::VmError::unavailable("the script runtime is not initialized"));
		}

		auto* host = runtime->host(context);
		if (!host)
		{
			return std::unexpected(vm::VmError::unavailable(
			    std::format("no script host is bound to DataModel type {}", std::to_underlying(context))));
		}

		return host;
	}

	std::expected<void, vm::VmError> schedule(const RBX::DataModelType context, const std::string_view source,
	                                          const std::string_view chunk_name)
	{
		auto host = host_or_error(context);
		if (!host)
		{
			return std::unexpected(std::move(host.error()));
		}

		auto chunk = build_chunk(source, chunk_name, false);
		if (!chunk)
		{
			return std::unexpected(std::move(chunk.error()));
		}

		(*host)->dispatcher().post(std::move(*chunk));
		return {};
	}

	std::expected<std::future<WorkResult>, vm::VmError> evaluate(const RBX::DataModelType context,
	                                                             const std::string_view source,
	                                                             const std::string_view chunk_name)
	{
		auto host = host_or_error(context);
		if (!host)
		{
			return std::unexpected(std::move(host.error()));
		}

		auto chunk = build_chunk(source, chunk_name, true);
		if (!chunk)
		{
			return std::unexpected(std::move(chunk.error()));
		}

		return (*host)->dispatcher().post_for_result(std::move(*chunk));
	}
}
