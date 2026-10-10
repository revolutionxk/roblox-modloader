#include "detours.hpp"

#include <functional>

namespace tracy_profiler
{
	static std::size_t thread_slot()
	{
		thread_local const auto slot = std::hash<std::thread::id>{}(std::this_thread::get_id());
		return slot;
	}

	InFlight::Call::Call(InFlight& in_flight) :
	    m_count(in_flight.m_slots[thread_slot() % in_flight.m_slots.size()].count)
	{
		m_count.fetch_add(1);
	}

	InFlight::Call::~Call()
	{
		m_count.fetch_sub(1);
	}

	void InFlight::wait_idle(const std::chrono::milliseconds timeout) const
	{
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		for (const auto& slot : m_slots)
		{
			while (slot.count.load() != 0 && std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
}
