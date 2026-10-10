#pragma once

#include "memory_engine.hpp"

#include <RobloxModLoader/platform/memory/import_slots.hpp>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace spdlog
{
	class logger;
}

namespace tracy_profiler
{
	class Callstacks;
	struct Settings;

	class HeapMemory
	{
	public:
		HeapMemory(const MemoryEngine& engine, const Callstacks& callstacks, std::shared_ptr<spdlog::logger> log);
		~HeapMemory();

		HeapMemory(const HeapMemory&) = delete;
		HeapMemory& operator=(const HeapMemory&) = delete;

		void apply(const Settings& settings);
		void stop();
		[[nodiscard]] bool active() const;
		[[nodiscard]] std::uint64_t take_events();
		[[nodiscard]] std::uint64_t dropped() const;

		void record_allocation(void* block, std::size_t size);
		void record_free(void* block);

	private:
		struct Binding
		{
			std::string_view symbol;
			std::vector<rml::platform::ImportSlot> slots;
			void* hook{};
		};

		bool bind();
		void unbind();
		void discard(const char* pool) const;
		void discard_all() const;

		const MemoryEngine& m_engine;
		const Callstacks& m_callstacks;
		std::shared_ptr<spdlog::logger> m_log;
		std::deque<std::string> m_names;
		std::vector<const char*> m_pools;
		std::vector<std::uint8_t> m_luau;
		std::vector<Binding> m_bindings;
		std::atomic<bool> m_events{false};
		std::atomic<bool> m_skip_luau{false};
		std::atomic<std::int32_t> m_depth{16};
		std::atomic<std::size_t> m_min_size{};
		std::uintptr_t m_image_begin{};
		std::size_t m_image_size{};
		bool m_bound{};
		bool m_free_detoured{};
		alignas(64) std::atomic<std::uint64_t> m_seen{};
		alignas(64) std::atomic<std::uint64_t> m_failures{};
	};
}
