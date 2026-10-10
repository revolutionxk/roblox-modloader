#pragma once

namespace tracy_profiler
{
	struct Engine;
	struct Settings;
	class Timers;
	class Callstacks;

	void install_capture(const Engine& engine, Timers& timers, const Callstacks& callstacks);
	void apply_settings(const Settings& settings);
	void remove_capture();
}
