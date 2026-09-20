#include <chrono>
#include <thread>
#include "RobloxModLoader/hooking/hooking.hpp"
#include "RobloxModLoader/logger/logger.hpp"
#include "RobloxModLoader/mod/mod_base.hpp"
#include "internal_pointers.hpp"

RML_LOG_SCOPE("InternalDeveloper")

namespace internal_developer
{
	static void enable_internal() noexcept
	{
		const auto& pointers = engine_pointers();

		*pointers.channel_flag = true;
		*pointers.internal_flag = true;
	}

#if !defined(RML_WINDOWS)
	static bool is_internal()
	{
		enable_internal();
		return rml::Hooking::get_original<&is_internal>()();
	}
#endif
}

class InternalDeveloperMod final : public ModBase
{
private:
#if defined(RML_WINDOWS)
	std::jthread m_flag_writer;
#endif

public:
	InternalDeveloperMod()
	{
		name = "Internal Developer Mod";
		version = "2.0.0";
		author = "Revolution";
		description = "Enables Roblox Studio internal developer features.";
	}

	void on_load() override
	{
		if (!internal_developer::resolve_engine_pointers())
			return;

#if defined(RML_WINDOWS)
		internal_developer::enable_internal();
		m_flag_writer = std::jthread([](const std::stop_token stop_token) {
			while (!stop_token.stop_requested())
			{
				internal_developer::enable_internal();
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			}
		});

		RML_INFO("Internal developer flags enabled and maintained");
#else
		rml::Hooking::DetourHookHelper::add<&internal_developer::is_internal>("IsInternal",
		    internal_developer::engine_pointers().is_internal);
		RML_INFO("Internal developer IsInternal hook registered");
#endif
	}

	void on_unload() override
	{
#if defined(RML_WINDOWS)
		if (m_flag_writer.joinable())
		{
			m_flag_writer.request_stop();
			m_flag_writer.join();
		}
#endif

		RML_INFO("Internal developer features disabled");
	}
};

extern "C"
{
	RML_MOD_ABI_EXPORT ModBase* start_mod()
	{
		return new InternalDeveloperMod();
	}

	RML_MOD_ABI_EXPORT void uninstall_mod(const ModBase* mod)
	{
		delete mod;
	}
}

RML_EXPORT_MOD_ABI_VERSION()
