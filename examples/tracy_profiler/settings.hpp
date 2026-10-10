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
		bool frame_images = true;
		std::int32_t frame_image_width = 320;
		std::int32_t frame_image_height = 180;
		std::int32_t frame_image_interval = 1;
		bool gpu_zones = true;
	};

	namespace keys
	{
		inline constexpr std::string_view enabled = "enabled";
		inline constexpr std::string_view pass_through = "pass_through";
		inline constexpr std::string_view zone_callstacks = "zone_callstacks";
		inline constexpr std::string_view callstack_depth = "callstack_depth";
		inline constexpr std::string_view sampling_hz = "sampling_hz";
		inline constexpr std::string_view frame_images = "frame_images";
		inline constexpr std::string_view frame_image_width = "frame_image_width";
		inline constexpr std::string_view frame_image_height = "frame_image_height";
		inline constexpr std::string_view frame_image_interval = "frame_image_interval";
		inline constexpr std::string_view gpu_zones = "gpu_zones";
	}

	inline constexpr std::int64_t max_callstack_depth = 62;

	inline std::string default_config_template()
	{
		return "enabled = true\n"
		       "pass_through = false\n"
		       "zone_callstacks = true\n"
		       "callstack_depth = 32\n"
		       "sampling_hz = 1000\n"
		       "frame_images = true\n"
		       "frame_image_width = 320\n"
		       "frame_image_height = 180\n"
		       "frame_image_interval = 1\n"
		       "gpu_zones = true\n";
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
		settings.frame_images = store.get_bool(keys::frame_images, settings.frame_images);
		settings.frame_image_width =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::frame_image_width, settings.frame_image_width), 16, 1024));
		settings.frame_image_height =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::frame_image_height, settings.frame_image_height), 16, 1024));
		settings.frame_image_interval =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::frame_image_interval, settings.frame_image_interval), 1, 60));
		settings.gpu_zones = store.get_bool(keys::gpu_zones, settings.gpu_zones);
		return settings;
	}
}
