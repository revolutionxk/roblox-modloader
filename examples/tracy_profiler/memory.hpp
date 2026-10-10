#pragma once

#include "memory_engine.hpp"

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

	private:
		const Callstacks& m_callstacks;
		std::shared_ptr<spdlog::logger> m_log;
		MemoryEngine m_engine;
	};
}
