#pragma once

#include <RobloxModLoader/roblox/profiler/micro_profile.hpp>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	struct ZoneHealth
	{
		std::atomic<std::int64_t> unbalanced{};
		std::atomic<std::int64_t> orphaned{};
		std::atomic<std::int64_t> overflowed{};
	};

	class ZoneStack
	{
	public:
		static constexpr std::size_t capacity = 256;

		struct Entry
		{
			MicroProfileTimerToken token;
			TracyCZoneCtx zone;
			const ___tracy_source_location_data* location;
		};

		void push(MicroProfileTimerToken token, TracyCZoneCtx zone, const ___tracy_source_location_data* location);
		void pop(MicroProfileTimerToken token);
		[[nodiscard]] const Entry* innermost() const;
		[[nodiscard]] std::string_view append_label(const Entry& entry, std::string_view text);

		[[nodiscard]] static ZoneStack& current();
		[[nodiscard]] static ZoneHealth& health();

	private:
		std::array<Entry, capacity> m_entries{};
		std::size_t m_depth{};
		std::size_t m_skipped{};
		std::string m_label;
		std::uint32_t m_label_zone{};
	};
}
