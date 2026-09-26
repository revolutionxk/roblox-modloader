#include "managed_bridge.hpp"

#include "RobloxModLoader/internal/common.hpp"

RML_LOG_SCOPE("ManagedBridge");

namespace rml::dotnet
{
	static constexpr host_string_view k_type = RML_HOST_STR("RML.NativeHost.NativeHost, RML.NativeHost");
	static constexpr host_string_view k_init = RML_HOST_STR("Initialize");
	static constexpr host_string_view k_load = RML_HOST_STR("LoadMod");
	static constexpr host_string_view k_unload = RML_HOST_STR("UnloadMod");
	static constexpr host_string_view k_shutdown = RML_HOST_STR("Shutdown");
	static constexpr host_string_view k_notify = RML_HOST_STR("NotifyDataModelChanged");

	std::expected<void, std::string> ManagedBridge::initialize(const std::filesystem::path& native_host_dll, const std::filesystem::path& mods_root)
	{
		auto init = m_runtime.get_function<InitFn>(native_host_dll, k_type, k_init);
		if (!init)
			return std::unexpected(init.error());

		auto load = m_runtime.get_function<LoadModFn>(native_host_dll, k_type, k_load);
		if (!load)
			return std::unexpected(load.error());

		auto unload = m_runtime.get_function<UnloadFn>(native_host_dll, k_type, k_unload);
		if (!unload)
			return std::unexpected(unload.error());

		auto shutdown = m_runtime.get_function<ShutdownFn>(native_host_dll, k_type, k_shutdown);
		if (!shutdown)
			return std::unexpected(shutdown.error());

		m_initialize = *init;
		m_load_mod   = *load;
		m_unload_mod = *unload;
		m_shutdown   = *shutdown;
		
		auto notify = m_runtime.get_function<NotifyDataModelFn>(native_host_dll, k_type, k_notify);
		if (notify)
		{
			m_notify_data_model = *notify;
		}

		auto* table     = m_registry.table();
		const auto root = mods_root.string();

		if (int32_t rc = m_initialize(root.c_str(), table); rc != 0)
			return std::unexpected(std::format("rml_initialize returned {}", rc));

		RML_INFO("C# side initialized");
		return {};
	}


std::expected<void, std::string> ManagedBridge::notify_data_model_changed(uint64_t old_dm, uint64_t new_dm, int32_t dm_type) const
{
	if (!m_notify_data_model)
		return std::unexpected("notify function not available");

	if (int32_t rc = m_notify_data_model(old_dm, new_dm, dm_type); rc != 0)
		return std::unexpected(std::format("notify_data_model_changed failed rc={}", rc));

	return {};
}

	std::expected<void, std::string> ManagedBridge::load_mod(const std::filesystem::path& path) const
	{
		if (!m_load_mod)
			return std::unexpected("Bridge not initialized");

		const auto p = path.string();
		if (int32_t rc = m_load_mod(p.c_str()); rc != 0)
			return std::unexpected(std::format("rml_load_mod failed for '{}' rc={}", path.filename().string(), rc));
		return {};
	}

	std::expected<void, std::string> ManagedBridge::unload_mod(const std::filesystem::path& path) const
	{
		if (!m_unload_mod)
			return std::unexpected("Bridge not initialized");

		const auto p = path.string();
		m_unload_mod(p.c_str());

		return {};
	}

	std::expected<void, std::string> ManagedBridge::shutdown() const
	{
		if (m_shutdown)
			m_shutdown();
		return {};
	}
} // namespace rml::dotnet
