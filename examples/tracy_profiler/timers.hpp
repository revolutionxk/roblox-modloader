#pragma once

#include <RobloxModLoader/roblox/profiler/micro_profile.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <tracy/TracyC.h>
#include <unordered_map>

namespace tracy_profiler
{
	class Symbols;

	class Timers
	{
	public:
		explicit Timers(MicroProfile& state);
		~Timers();

		Timers(const Timers&) = delete;
		Timers& operator=(const Timers&) = delete;

		void attach(const Symbols& symbols);
		[[nodiscard]] const ___tracy_source_location_data* source_location(MicroProfileTimerToken token, std::uintptr_t caller);
		void record_owner(MicroProfileTimerToken token, std::uintptr_t caller);
		struct Scope
		{
			std::uintptr_t site;
			std::string name;
		};

		[[nodiscard]] std::optional<Scope> scope_within(std::uintptr_t begin, std::uintptr_t end) const;
		[[nodiscard]] bool shared_site(std::uintptr_t site) const;
		void emit_counters();

	private:
		struct Location
		{
			std::uintptr_t caller;
			std::string name;
			std::string group;
			std::string function;
			std::string file;
			___tracy_source_location_data data;
			Location* next;
		};

		struct Site
		{
			std::uint32_t timer;
			bool shared;
		};

		struct Counter
		{
			std::string name;
			std::int64_t last;
		};

		[[nodiscard]] static const Location* find(const Location* first, std::uintptr_t caller);
		[[nodiscard]] std::string timer_name(std::uint32_t index) const;
		[[nodiscard]] std::string group_name(std::uint32_t index) const;
		void register_site(std::uint32_t index, std::uintptr_t caller);
		[[nodiscard]] std::string counter_path(std::uint32_t index) const;

		MicroProfile& m_state;
		const Symbols* m_symbols{};
		std::array<std::atomic<Location*>, MICROPROFILE_MAX_TIMERS> m_locations{};
		std::atomic<std::size_t> m_location_count{};
		mutable std::shared_mutex m_sites_mutex;
		std::map<std::uintptr_t, Site> m_sites;
		std::unordered_map<std::uintptr_t, std::uint32_t> m_owners;
		std::array<std::unique_ptr<Counter>, MICROPROFILE_MAX_COUNTERS> m_counters{};
	};
}
