#pragma once

#include "external_memory.hpp"
#include "heap_memory.hpp"
#include "luau_frames.hpp"
#include "luau_memory.hpp"
#include "memory_engine.hpp"
#include "memory_plots.hpp"

#include <chrono>
#include <memory>

namespace spdlog
{
	class logger;
}

namespace tracy_profiler
{
	class Callstacks;
	struct Settings;

	class Memory
	{
	public:
		Memory(const Callstacks& callstacks, std::shared_ptr<spdlog::logger> log);
		~Memory();

		Memory(const Memory&) = delete;
		Memory& operator=(const Memory&) = delete;

		void start(const Settings& settings);
		void apply(const Settings& settings);
		void stop();

		[[nodiscard]] const LuauFrames& luau_frames() const
		{
			return *m_frames;
		}

	private:
		[[nodiscard]] std::uint64_t dropped() const;

		const Callstacks& m_callstacks;
		std::shared_ptr<spdlog::logger> m_log;
		MemoryEngine m_engine;
		std::unique_ptr<LuauFrames> m_frames;
		std::unique_ptr<LuauMemory> m_luau;
		std::unique_ptr<ExternalMemory> m_external;
		std::unique_ptr<HeapMemory> m_heap;
		std::uint32_t m_ticks{};
		std::chrono::steady_clock::time_point m_rate_since{};
		std::unique_ptr<MemoryPlots> m_plots;
	};
}
