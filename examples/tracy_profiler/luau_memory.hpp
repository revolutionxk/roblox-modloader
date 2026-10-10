#pragma once

#include "memory_engine.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace spdlog
{
	class logger;
}

namespace tracy_profiler
{
	class Callstacks;
	class LuauFrames;
	struct Settings;

	class LuauMemory
	{
	public:
		struct Sample
		{
			const char* total{};
			std::size_t total_bytes{};
			std::vector<std::pair<const char*, std::size_t>> categories;
		};

		LuauMemory(const MemoryEngine& engine, const Callstacks& callstacks, LuauFrames& frames, std::shared_ptr<spdlog::logger> log);
		~LuauMemory();

		LuauMemory(const LuauMemory&) = delete;
		LuauMemory& operator=(const LuauMemory&) = delete;

		void install();
		void apply(const Settings& settings);
		void maintain();
		void remove();
		void sample(std::vector<Sample>& samples);
		[[nodiscard]] std::size_t hooked() const;

		void add(rml::luau::mirror::GlobalState* global);
		void begin_close(rml::luau::mirror::GlobalState* global);
		void end_close(rml::luau::mirror::GlobalState* global);
		void name_category(rml::luau::mirror::GlobalState* global, std::uint8_t index, const std::string& name);
		void emit_allocation(rml::luau::mirror::LuaState* state, void* block, std::size_t size, bool walk);
		void emit_free(rml::luau::mirror::LuaState* state, void* block);

	private:
		struct Vm
		{
			rml::luau::mirror::GlobalState* global{};
			std::string label;
			const char* pool{};
			const char* total{};
			std::array<std::string, 256> names;
			std::array<const char*, 256> plots{};
			bool closing{};
			bool hooked{};
			bool yielded{};
		};

		struct Route
		{
			std::atomic<rml::luau::mirror::GlobalState*> global{};
			std::atomic<const char*> pool{};
		};

		[[nodiscard]] Vm* find(const rml::luau::mirror::GlobalState* global);
		Vm& ensure(rml::luau::mirror::GlobalState* global);
		const char* intern(std::string text);
		[[nodiscard]] bool hook(Vm& vm) const;
		void unhook(Vm& vm) const;
		[[nodiscard]] const char* pool_of(const rml::luau::mirror::GlobalState* global) const;

		const MemoryEngine& m_engine;
		const Callstacks& m_callstacks;
		LuauFrames& m_frames;
		std::shared_ptr<spdlog::logger> m_log;
		mutable std::mutex m_mutex;
		std::deque<Vm> m_vms;
		std::deque<std::string> m_names;
		std::array<Route, 256> m_routes{};
		std::atomic<std::size_t> m_route_limit{};
		std::atomic<bool> m_events{false};
		std::atomic<std::int32_t> m_native_depth{16};
		std::atomic<std::int32_t> m_luau_depth{8};
		std::atomic<std::size_t> m_hooked{};
		std::uint32_t m_next_id{1};
		bool m_installed{};
		bool m_names_hooked{};
	};
}
