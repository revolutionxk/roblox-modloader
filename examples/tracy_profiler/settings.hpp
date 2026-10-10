#pragma once

#include <RobloxModLoader/config/mod_settings.hpp>
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

namespace tracy_profiler
{
	struct Settings
	{
		bool enabled = true;
		bool pass_through = false;
		bool zone_callstacks = true;
		std::int32_t callstack_depth = 32;
		std::int32_t sampling_hz = 1000;
	};

	namespace keys
	{
		inline constexpr std::string_view enabled = "enabled";
		inline constexpr std::string_view pass_through = "pass_through";
		inline constexpr std::string_view zone_callstacks = "zone_callstacks";
		inline constexpr std::string_view callstack_depth = "callstack_depth";
		inline constexpr std::string_view sampling_hz = "sampling_hz";
	}

	inline constexpr std::int64_t max_callstack_depth = 62;

	inline std::string default_config_template()
	{
		return "enabled = true\n"
		       "pass_through = false\n"
		       "zone_callstacks = true\n"
		       "callstack_depth = 32\n"
		       "sampling_hz = 1000\n";
	}

	inline Settings read(const rml::config::ModSettings& store)
	{
		Settings settings;
		settings.enabled = store.get_bool(keys::enabled, settings.enabled);
		settings.pass_through = store.get_bool(keys::pass_through, settings.pass_through);
		settings.zone_callstacks = store.get_bool(keys::zone_callstacks, settings.zone_callstacks);
		settings.callstack_depth =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::callstack_depth, settings.callstack_depth), 0, max_callstack_depth));
		settings.sampling_hz =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::sampling_hz, settings.sampling_hz), 100, 20000));
		return settings;
	}
}
