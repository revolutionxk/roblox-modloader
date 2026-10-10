#include "detours.hpp"

#include <functional>

namespace tracy_profiler
{
	InFlight::Call::Call(InFlight& in_flight) :
	    m_count(in_flight.m_slots[std::hash<std::thread::id>{}(std::this_thread::get_id()) % in_flight.m_slots.size()].count)
	{
		m_count.fetch_add(1);
	}

	InFlight::Call::~Call()
	{
		m_count.fetch_sub(1);
	}

	bool InFlight::wait_idle(const std::chrono::milliseconds timeout) const
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		for (const auto& slot : m_slots)
		{
			while (slot.count.load() != 0)
			{
				if (std::chrono::steady_clock::now() >= deadline)
					return false;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		return true;
	}
}
