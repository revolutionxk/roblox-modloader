#include "dotnet_mod_loader.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"
#include "RobloxModLoader/roblox/task_scheduler.hpp"
#include "RobloxModLoader/roblox/job_manager.hpp"
#include "instance_collector_job.hpp"
#include "roblox_interop_provider.hpp"

RML_LOG_SCOPE("DotnetModLoader");

namespace rml::dotnet
{
	DotnetModLoader::DotnetModLoader(const std::filesystem::path& runtime_path, const std::filesystem::path& mods_root) :
	    m_bridge(m_runtime, m_registry),
	    m_native_host_dll(runtime_path / "RML.NativeHost.dll"),
	    m_runtime_config(runtime_path / "RML.runtimeconfig.json"),
	    m_mods_root(mods_root)
	{
	}

	std::expected<void, std::string> DotnetModLoader::ensure_initialized()
	{
		if (m_initialized)
			return {};

		RobloxInteropProvider::populate(*m_registry.table());

		if (auto r = m_runtime.initialize(m_runtime_config); !r)
			return r;

		if (auto r = m_bridge.initialize(m_native_host_dll, m_mods_root); !r)
			return r;

		m_initialized = true;

		if (jobs::has_job_manager())
			jobs::job_manager().register_job_and_ignore<InstanceCollectorJob>();

		g_dotnet_mod_loader = this;
		return {};
	}

	std::expected<void, std::string> DotnetModLoader::load(const std::filesystem::path& path)
	{
		if (auto r = ensure_initialized(); !r)
			return r;
		if (auto r = m_bridge.load_mod(path); !r)
			return r;

		if (rml::has_task_scheduler())
		{
			for (int i = 0; i <= static_cast<int>(RBX::DataModelType::Standalone); ++i)
			{
				const auto current = rml::task_scheduler().get_data_model_by_type(static_cast<RBX::DataModelType>(i));
				if (current)
				{
					if (auto r2 = m_bridge.notify_data_model_changed(0, reinterpret_cast<uint64_t>(static_cast<const RBX::Instance*>(current)), i); !r2)
					{
						RML_WARN("notify_data_model_changed failed: {}", r2.error());
					}
				}
			}
		}

		return {};
	}

	std::expected<void, std::string> DotnetModLoader::unload(const std::filesystem::path& path)
	{
		if (!m_initialized)
			return {};
		return m_bridge.unload_mod(path);
	}

	std::expected<void, std::string> DotnetModLoader::reload(const std::filesystem::path& path)
	{
		if (auto r = unload(path); !r)
			return r;
		return load(path);
	}

	void DotnetModLoader::unload_all()
	{
		if (!m_initialized)
			return;

		m_bridge.shutdown();
		g_dotnet_mod_loader = nullptr;
	}


	void DotnetModLoader::notify_data_model_changed(uint64_t old_dm, uint64_t new_dm, int32_t dm_type) const
	{
		if (auto r = m_bridge.notify_data_model_changed(old_dm, new_dm, dm_type); !r)
		{
			RML_WARN("notify_data_model_changed failed: {}", r.error());
		}
	}
} // namespace rml::dotnet
