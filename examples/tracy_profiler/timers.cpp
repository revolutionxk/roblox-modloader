#include "timers.hpp"

#include "symbols.hpp"

#include <RobloxModLoader/memory/string_anchor.hpp>
#include <RobloxModLoader/platform/memory/host_image.hpp>
#include <algorithm>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace tracy_profiler
{
	static constexpr std::size_t max_locations = 16384;

	Timers::Timers(MicroProfile& state) :
	    m_state(state)
	{
	}

	Timers::~Timers()
	{
		for (auto& head : m_locations)
		{
			for (const auto* location = head.load(std::memory_order_acquire); location;)
				delete std::exchange(location, location->next);
		}
	}

	void Timers::attach(const Symbols& symbols)
	{
		m_symbols = &symbols;
	}

	const Timers::Location* Timers::find(const Location* first, const std::uintptr_t caller)
	{
		for (const auto* location = first; location; location = location->next)
		{
			if (location->caller == caller)
				return location;
		}
		return nullptr;
	}

	std::string Timers::timer_name(const std::uint32_t index) const
	{
		const auto& timer = m_state.timer_info[index];
		return {timer.name, std::min<std::size_t>(timer.name_len, MICROPROFILE_NAME_MAX_LEN)};
	}

	std::string Timers::group_name(const std::uint32_t index) const
	{
		const auto& timer = m_state.timer_info[index];
		if (timer.group_index >= MICROPROFILE_MAX_GROUPS)
			return {};

		const auto& group = m_state.group_info[timer.group_index];
		return {group.name, std::min<std::size_t>(group.name_len, MICROPROFILE_NAME_MAX_LEN)};
	}

	void Timers::register_site(const std::uint32_t index, const std::uintptr_t caller)
	{
		std::unique_lock lock(m_sites_mutex);
		const auto [site, inserted] = m_sites.try_emplace(caller, Site{index, false});
		if (!inserted && site->second.timer != index)
			site->second.shared = true;
	}

	const ___tracy_source_location_data* Timers::source_location(const MicroProfileTimerToken token, const std::uintptr_t caller)
	{
		const auto index = MicroProfileGetTimerIndex(token);
		if (index >= MICROPROFILE_MAX_TIMERS || index >= std::atomic_ref(m_state.total_timers).load(std::memory_order_acquire))
			return nullptr;

		auto& head = m_locations[index];
		auto* first = head.load(std::memory_order_acquire);
		if (const auto* existing = find(first, caller))
			return &existing->data;
		if (first && m_location_count.load(std::memory_order_relaxed) >= max_locations)
			return &first->data;

		if (caller)
			register_site(index, caller);

		auto location = std::make_unique<Location>();
		location->caller = caller;
		location->name = timer_name(index);
		location->group = group_name(index);

		const auto image = rml::platform::studio_image_name();
		if (caller && m_symbols)
		{
			location->function = m_symbols->function_name(caller);
			location->file = std::format("{}+0x{:x} ({})", image, caller - m_symbols->image_base(), location->group);
		}
		else
		{
			location->function = location->group;
			location->file = std::string(image);
		}
		location->data = {location->name.c_str(),
		    location->function.c_str(),
		    location->file.c_str(),
		    0,
		    m_state.timer_info[index].color};

		location->next = first;
		while (!head.compare_exchange_weak(location->next, location.get(), std::memory_order_acq_rel, std::memory_order_acquire))
		{
			if (const auto* existing = find(location->next, caller))
				return &existing->data;
		}
		m_location_count.fetch_add(1, std::memory_order_relaxed);
		return &location.release()->data;
	}

	void Timers::record_owner(const MicroProfileTimerToken token, const std::uintptr_t caller)
	{
		const auto index = MicroProfileGetTimerIndex(token);
		if (!caller || index >= MICROPROFILE_MAX_TIMERS)
			return;

		const auto function = rml::memory::function_containing(reinterpret_cast<const void*>(caller - 1));
		if (!function)
			return;

		std::unique_lock lock(m_sites_mutex);
		m_owners.try_emplace(reinterpret_cast<std::uintptr_t>(function->start), index);
	}

	std::optional<Timers::Scope> Timers::scope_within(const std::uintptr_t begin, const std::uintptr_t end) const
	{
		std::shared_lock lock(m_sites_mutex);
		if (const auto owner = m_owners.find(begin); owner != m_owners.end())
			return Scope{0, std::format("{}/{}", group_name(owner->second), timer_name(owner->second))};

		for (auto site = m_sites.upper_bound(begin); site != m_sites.end() && site->first <= end; ++site)
		{
			if (!site->second.shared)
				return Scope{site->first, std::format("{}/{}", group_name(site->second.timer), timer_name(site->second.timer))};
		}
		return std::nullopt;
	}

	bool Timers::shared_site(const std::uintptr_t site) const
	{
		std::shared_lock lock(m_sites_mutex);
		const auto it = m_sites.find(site);
		return it != m_sites.end() && it->second.shared;
	}

	std::string Timers::counter_path(const std::uint32_t index) const
	{
		std::vector<std::string_view> parts;
		for (auto current = static_cast<std::int32_t>(index);
		    current >= 0 && static_cast<std::uint32_t>(current) < MICROPROFILE_MAX_COUNTERS && parts.size() < 32;
		    current = m_state.counter_info[current].parent)
		{
			const auto& info = m_state.counter_info[current];
			parts.emplace_back(info.name ? std::string_view(info.name, info.name_len) : std::string_view{});
		}

		std::string path = "Counters";
		for (auto it = parts.rbegin(); it != parts.rend(); ++it)
		{
			path += '/';
			path += *it;
		}
		return path;
	}

	void Timers::emit_counters()
	{
		const auto count = std::min<std::uint32_t>(std::atomic_ref(m_state.num_counters).load(std::memory_order_acquire), MICROPROFILE_MAX_COUNTERS);
		for (std::uint32_t index = 0; index < count; ++index)
		{
			const auto value = m_state.counters[index].load(std::memory_order_relaxed);
			auto& counter = m_counters[index];
			if (!counter)
			{
				counter = std::make_unique<Counter>(Counter{counter_path(index), value});
				const auto type = m_state.counter_info[index].format == MICROPROFILE_COUNTER_FORMAT_BYTES ? TracyPlotFormatMemory : TracyPlotFormatNumber;
				___tracy_emit_plot_config(counter->name.c_str(), type, 0, 1, 0);
			}
			else if (counter->last == value)
				continue;

			counter->last = value;
			___tracy_emit_plot_int(counter->name.c_str(), value);
		}
	}
}
