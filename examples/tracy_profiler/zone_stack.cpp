#include "zone_stack.hpp"

namespace tracy_profiler
{
	static ZoneHealth g_health;

	ZoneStack& ZoneStack::current()
	{
		thread_local ZoneStack stack;
		return stack;
	}

	ZoneHealth& ZoneStack::health()
	{
		return g_health;
	}

	void ZoneStack::push(const MicroProfileTimerToken token, const TracyCZoneCtx zone, const ___tracy_source_location_data* location)
	{
		if (m_depth == capacity)
		{
			++m_skipped;
			g_health.overflowed.fetch_add(1, std::memory_order_relaxed);
			return;
		}

		m_entries[m_depth++] = {token, zone, location};
	}

	void ZoneStack::pop(const MicroProfileTimerToken token)
	{
		if (m_skipped > 0)
		{
			--m_skipped;
			return;
		}

		for (auto depth = m_depth; depth > 0; --depth)
		{
			if (m_entries[depth - 1].token != token)
				continue;

			if (depth != m_depth)
				g_health.unbalanced.fetch_add(1, std::memory_order_relaxed);

			while (m_depth >= depth)
				___tracy_emit_zone_end(m_entries[--m_depth].zone);
			return;
		}

		g_health.orphaned.fetch_add(1, std::memory_order_relaxed);
	}

	const ZoneStack::Entry* ZoneStack::innermost() const
	{
		return m_depth > 0 ? &m_entries[m_depth - 1] : nullptr;
	}

	std::string_view ZoneStack::append_label(const Entry& entry, const std::string_view text)
	{
		if (m_label_zone != entry.zone.id || m_label.empty())
		{
			m_label = entry.location && entry.location->name ? entry.location->name : "";
			m_label_zone = entry.zone.id;
		}

		m_label += ' ';
		m_label += text;
		return m_label;
	}
}
