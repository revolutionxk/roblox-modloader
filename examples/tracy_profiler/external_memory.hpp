#pragma once

#include "memory_engine.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace spdlog
{
	class logger;
}

namespace tracy_profiler
{
	class Callstacks;
	struct Settings;

	class ExternalMemory
	{
	public:
		ExternalMemory(const MemoryEngine& engine, const Callstacks& callstacks, std::shared_ptr<spdlog::logger> log);
		~ExternalMemory();

		ExternalMemory(const ExternalMemory&) = delete;
		ExternalMemory& operator=(const ExternalMemory&) = delete;

		[[nodiscard]] bool install();
		void apply(const Settings& settings);
		void remove();
		[[nodiscard]] std::uint64_t dropped() const;

		[[nodiscard]] bool recording() const;
		[[nodiscard]] bool from_flush() const;
		void allocate(std::uint32_t category, RBX::Memory::MemoryType type, std::uintptr_t id, std::size_t size);
		void release(std::uint32_t category, RBX::Memory::MemoryType type, std::uintptr_t id, std::size_t size);
		void count_failure();

	private:
		static constexpr std::uint64_t address_base = 0x0040'0000'0000'0000;

		struct Resource
		{
			std::uint32_t category{};
			RBX::Memory::MemoryType type{};
			std::uintptr_t id{};

			bool operator==(const Resource&) const = default;
		};

		struct ResourceHash
		{
			std::size_t operator()(const Resource& resource) const noexcept;
		};

		struct Live
		{
			std::uint64_t address{};
			std::size_t size{};
		};

		using LiveMap = std::unordered_map<Resource, std::vector<Live>, ResourceHash>;

		[[nodiscard]] const char* pool(std::uint32_t category, RBX::Memory::MemoryType type) const;
		[[nodiscard]] LiveMap::iterator newest_with_size(std::uint32_t category, RBX::Memory::MemoryType type, std::size_t size);
		void observe_epoch();
		void forget_all();

		const MemoryEngine& m_engine;
		const Callstacks& m_callstacks;
		std::shared_ptr<spdlog::logger> m_log;
		std::vector<std::string> m_gpu_pools;
		std::vector<std::string> m_heap_pools;
		std::mutex m_mutex;
		LiveMap m_live;
		std::uint64_t m_next{address_base};
		std::uint64_t m_epoch{};
		std::atomic<bool> m_events{false};
		std::atomic<std::int32_t> m_depth{16};
		std::atomic<std::uint64_t> m_failures{};
		bool m_installed{};
	};
}
