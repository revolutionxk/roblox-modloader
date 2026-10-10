#pragma once

#include <cstdint>
#include <filesystem>
#include <shared_mutex>
#include <string>
#include <thread>
#include <tracy/TracyC.h>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tracy_profiler
{
	class Timers;

	class Symbols
	{
	public:
		Symbols(const Timers& timers, std::uintptr_t image_base);
		~Symbols();

		Symbols(const Symbols&) = delete;
		Symbols& operator=(const Symbols&) = delete;

		void load_file(const std::filesystem::path& path);
		void add(const void* function, std::string name);
		void index_rtti();
		void index_scope_owners(const void* get_token);
		[[nodiscard]] std::string function_name(std::uintptr_t address) const;
		[[nodiscard]] std::uintptr_t image_base() const;
		[[nodiscard]] std::size_t scope_owner_count() const;

		static int32_t tracy_resolver(uint64_t address, ___tracy_resolved_symbol* symbol, void* user);

	private:
		[[nodiscard]] std::string name_of(std::uintptr_t start, std::size_t size) const;
		[[nodiscard]] bool resolve(std::uint64_t address, ___tracy_resolved_symbol& symbol) const;

		const Timers& m_timers;
		std::uintptr_t m_image_base;
		mutable std::shared_mutex m_mutex;
		std::unordered_map<std::uintptr_t, std::string> m_names;
		std::vector<std::uintptr_t> m_scope_owners;
		mutable std::unordered_map<std::uintptr_t, std::pair<std::uintptr_t, std::string>> m_scope_names;
		std::jthread m_rtti;
	};
}
