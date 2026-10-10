#pragma once

#include "luau_memory.hpp"
#include "memory_engine.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tracy_profiler
{
	struct Settings;

	class MemoryPlots
	{
	public:
		MemoryPlots(const MemoryEngine& engine, LuauMemory* luau, std::function<std::uint64_t()> dropped, std::function<void()> tick);
		~MemoryPlots();

		MemoryPlots(const MemoryPlots&) = delete;
		MemoryPlots& operator=(const MemoryPlots&) = delete;

		void apply(const Settings& settings);
		void start();
		void stop();

	private:
		static constexpr auto sample_period = std::chrono::milliseconds(250);

		void run(std::stop_token stop);
		void sample();
		void sample_heap();
		void sample_gpu();
		void sample_luau();
		void sample_health();
		void plot(const char* name, std::uint64_t value);
		const char* datamodel_plot(std::uint32_t tag, std::uint32_t category);

		const MemoryEngine& m_engine;
		LuauMemory* m_luau;
		std::function<std::uint64_t()> m_dropped;
		std::function<void()> m_tick;
		std::vector<std::string> m_categories;
		std::vector<std::string> m_heap_plots;
		std::vector<std::string> m_gpu_plots;
		std::deque<std::string> m_names;
		std::unordered_map<std::uint32_t, const char*> m_datamodel_plots;
		std::unordered_map<const char*, std::uint64_t> m_last;
		std::unordered_set<const char*> m_configured;
		std::vector<std::uint64_t> m_heap_bytes;
		std::vector<std::uint64_t> m_datamodel_bytes;
		std::vector<LuauMemory::Sample> m_luau_samples;
		std::atomic<bool> m_enabled{true};
		std::atomic<bool> m_by_datamodel{false};
		std::uint64_t m_epoch{};
		std::mutex m_wait_mutex;
		std::condition_variable_any m_wait;
		std::jthread m_thread;
	};
}
