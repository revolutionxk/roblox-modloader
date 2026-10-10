#include "memory.hpp"

#include "callstack.hpp"
#include "settings.hpp"

#include <chrono>
#include <spdlog/spdlog.h>
#include <string>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	static std::string joined(const std::vector<std::string>& names)
	{
		std::string text;
		for (const auto& name : names)
		{
			if (!text.empty())
				text += ", ";
			text += name;
		}
		return text;
	}

	Memory::Memory(const Callstacks& callstacks, std::shared_ptr<spdlog::logger> log) :
	    m_callstacks(callstacks),
	    m_log(std::move(log)),
	    m_frames(std::make_unique<LuauFrames>())
	{
	}

	Memory::~Memory() = default;

	void Memory::start(const Settings& settings)
	{
		m_engine = resolve_memory_engine();
		if (const auto missing = m_engine.missing(); !missing.empty())
			m_log->warn("memory entry points missing ({}); the sources that need them stay off", joined(missing));
		if (m_engine.has_luau())
		{
			m_luau = std::make_unique<LuauMemory>(m_engine, m_callstacks, *m_frames, m_log);
			m_luau->install();
		}
		else
			m_log->warn("Luau memory off: LUA_NEWSTATE or LUA_CLOSE missing");
		if (m_engine.has_external())
		{
			m_external = std::make_unique<ExternalMemory>(m_engine, m_callstacks);
			if (!m_external->install())
			{
				m_log->warn("external memory events off: trackExternal entry points or memory categories missing");
				m_external.reset();
			}
		}
		else
			m_log->warn("external memory events off: trackExternal entry points missing");
		if (m_engine.has_heap())
			m_heap = std::make_unique<HeapMemory>(m_engine, m_callstacks, m_log);
		else
			m_log->warn("heap memory events off: allocator exports or category entry points missing");
		m_plots = std::make_unique<MemoryPlots>(
		    m_engine,
		    m_luau.get(),
		    [this] {
			    return dropped();
		    },
		    [this] {
			    if (!m_heap || !m_heap->active())
			    {
				    m_ticks = 0;
				    m_rate_since = {};
				    return;
			    }
			    const auto now = std::chrono::steady_clock::now();
			    if (m_rate_since == std::chrono::steady_clock::time_point{})
			    {
				    static_cast<void>(m_heap->take_events());
				    m_rate_since = now;
				    return;
			    }
			    if (++m_ticks % 40 != 0)
				    return;
			    const auto events = m_heap->take_events();
			    const auto seconds = std::chrono::duration<double>(now - m_rate_since).count();
			    m_rate_since = now;
			    if (___tracy_connected())
				    m_log->info("heap memory events: {:.0f}/s", static_cast<double>(events) / seconds);
		    });
		apply(settings);
		m_plots->start();
	}

	void Memory::apply(const Settings& settings)
	{
		if (m_plots)
			m_plots->apply(settings);
		if (m_luau)
			m_luau->apply(settings);
		if (m_external)
			m_external->apply(settings);
		if (m_heap)
			m_heap->apply(settings);
	}

	void Memory::stop()
	{
		if (m_plots)
			m_plots->stop();
		if (m_heap)
			m_heap->stop();
		if (m_external)
			m_external->remove();
		if (m_luau)
			m_luau->remove();
	}

	std::uint64_t Memory::dropped() const
	{
		return m_frames->dropped() + (m_external ? m_external->dropped() : 0) + (m_heap ? m_heap->dropped() : 0);
	}
}
