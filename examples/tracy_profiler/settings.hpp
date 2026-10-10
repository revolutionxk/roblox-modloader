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
		bool memory_plots = true;
		bool memory_plots_by_datamodel = false;
		bool memory_events_luau = false;
		bool memory_events_external = false;
		bool memory_events_heap = false;
		std::int32_t memory_callstack_depth = 16;
		std::int32_t memory_luau_depth = 8;
		std::int64_t memory_events_min_size = 0;
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
		inline constexpr std::string_view memory_plots = "memory_plots";
		inline constexpr std::string_view memory_plots_by_datamodel = "memory_plots_by_datamodel";
		inline constexpr std::string_view memory_events_luau = "memory_events_luau";
		inline constexpr std::string_view memory_events_external = "memory_events_external";
		inline constexpr std::string_view memory_events_heap = "memory_events_heap";
		inline constexpr std::string_view memory_callstack_depth = "memory_callstack_depth";
		inline constexpr std::string_view memory_luau_depth = "memory_luau_depth";
		inline constexpr std::string_view memory_events_min_size = "memory_events_min_size";
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
		       "gpu_zones = true\n"
		       "memory_plots = true\n"
		       "memory_plots_by_datamodel = false\n"
		       "memory_events_luau = false\n"
		       "memory_events_external = false\n"
		       "memory_events_heap = false\n"
		       "memory_callstack_depth = 16\n"
		       "memory_luau_depth = 8\n"
		       "memory_events_min_size = 0\n";
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
		settings.memory_plots = store.get_bool(keys::memory_plots, settings.memory_plots);
		settings.memory_plots_by_datamodel = store.get_bool(keys::memory_plots_by_datamodel, settings.memory_plots_by_datamodel);
		settings.memory_events_luau = store.get_bool(keys::memory_events_luau, settings.memory_events_luau);
		settings.memory_events_external = store.get_bool(keys::memory_events_external, settings.memory_events_external);
		settings.memory_events_heap = store.get_bool(keys::memory_events_heap, settings.memory_events_heap);
		settings.memory_callstack_depth =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::memory_callstack_depth, settings.memory_callstack_depth), 0, max_callstack_depth));
		settings.memory_luau_depth =
		    static_cast<std::int32_t>(std::clamp<std::int64_t>(store.get_int(keys::memory_luau_depth, settings.memory_luau_depth), 0, 32));
		settings.memory_events_min_size = std::clamp<std::int64_t>(store.get_int(keys::memory_events_min_size, settings.memory_events_min_size), 0, std::int64_t{1} << 30);
		return settings;
	}
}
