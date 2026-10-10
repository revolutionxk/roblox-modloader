#include "memory.hpp"

#include "callstack.hpp"
#include "settings.hpp"

#include <spdlog/spdlog.h>
#include <string>

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
		m_plots = std::make_unique<MemoryPlots>(
		    m_engine,
		    m_luau.get(),
		    [this] {
			    return dropped();
		    },
		    [] {
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
	}

	void Memory::stop()
	{
		if (m_plots)
			m_plots->stop();
		if (m_external)
			m_external->remove();
		if (m_luau)
			m_luau->remove();
	}

	std::uint64_t Memory::dropped() const
	{
		return m_frames->dropped() + (m_external ? m_external->dropped() : 0);
	}
}
