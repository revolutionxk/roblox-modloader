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
	    m_log(std::move(log))
	{
	}

	Memory::~Memory() = default;

	void Memory::start(const Settings& settings)
	{
		m_engine = resolve_memory_engine();
		if (const auto missing = m_engine.missing(); !missing.empty())
			m_log->warn("memory entry points missing ({}); the sources that need them stay off", joined(missing));
		apply(settings);
	}

	void Memory::apply(const Settings&)
	{
	}

	void Memory::stop()
	{
	}
}
