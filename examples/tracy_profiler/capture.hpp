#pragma once

#include <cstdint>

namespace tracy_profiler
{
	struct Engine;
	struct Settings;
	class Timers;
	class Callstacks;
	class GpuZones;

	void install_capture(const Engine& engine, Timers& timers, const Callstacks& callstacks);
	void apply_settings(const Settings& settings);
	void remove_capture();
	void attach_gpu_zones(GpuZones* zones);
	[[nodiscard]] std::uint64_t frame_marks() noexcept;
	[[nodiscard]] std::uint64_t connection_epoch() noexcept;
}
