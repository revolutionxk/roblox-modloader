#include "luau_frames.hpp"

#include <RobloxModLoader/luau/generated/layout_access.hpp>
#include <algorithm>
#include <cstdio>
#include <lua.h>
#include <string_view>
#include <thread>

namespace tracy_profiler
{
	using namespace rml::luau::mirror;

	static std::uint64_t mix(std::uint64_t value)
	{
		value ^= value >> 33;
		value *= 0xFF51AFD7ED558CCDULL;
		value ^= value >> 33;
		value *= 0xC4CEB9FE1A85EC53ULL;
		value ^= value >> 33;
		return value;
	}

	static std::string_view script_of(std::string_view source)
	{
		if (!source.empty() && (source.front() == '=' || source.front() == '@'))
			source.remove_prefix(1);
		return source;
	}

	static std::uint32_t current_line(const CallInfo& frame, const Proto& proto)
	{
		if (!proto.lineinfo || !proto.abslineinfo || !proto.code || proto.sizecode <= 0)
			return static_cast<std::uint32_t>(std::max(proto.linedefined, 0));
		const auto pc = frame.savedpc ? static_cast<std::int64_t>(frame.savedpc - proto.code) - 1 : 0;
		const auto index = std::clamp<std::int64_t>(pc, 0, proto.sizecode - 1);
		const auto gap = static_cast<int>(proto.linegaplog2) & 31;
		return static_cast<std::uint32_t>(proto.abslineinfo[index >> gap] + proto.lineinfo[index]);
	}

	template<std::size_t Size>
	static void write(std::array<char, Size>& target, const std::string_view text)
	{
		const auto length = std::min(text.size(), Size - 1);
		std::copy_n(text.data(), length, target.data());
		target[length] = '\0';
	}

	LuauFrames::LuauFrames() :
	    m_entries(std::make_unique<Entry[]>(capacity))
	{
	}

	std::uint64_t LuauFrames::dropped() const
	{
		return m_dropped.load(std::memory_order_relaxed);
	}

	std::uint64_t LuauFrames::frame_address(const CallInfo& frame)
	{
		const auto& closure = *rml::luau::access::closure_in(frame.func);
		const Proto* proto = closure.isC ? nullptr : closure.p;
		const auto line = proto ? current_line(frame, *proto) : 0;
		const auto identity = proto ?
		    mix(reinterpret_cast<std::uintptr_t>(proto)) ^ mix(reinterpret_cast<std::uintptr_t>(proto->source))
		        ^ mix(static_cast<std::uint64_t>(proto->linedefined) << 1) :
		    mix(reinterpret_cast<std::uintptr_t>(closure.debugname)) ^ mix(reinterpret_cast<std::uintptr_t>(closure.p) | 1);
		const auto key = mix(identity ^ line) | 1;

		auto index = (key >> 1) % capacity;
		for (std::size_t probe = 0; probe < probe_limit; ++probe, index = (index + 1) % capacity)
		{
			auto& entry = m_entries[index];
			auto current = entry.key.load(std::memory_order_acquire);
			if (current == 0 && entry.key.compare_exchange_strong(current, key, std::memory_order_acq_rel))
			{
				if (proto)
				{
					const auto function = rml::luau::access::text(proto->debugname);
					const auto script = script_of(rml::luau::access::text(proto->source));
					std::snprintf(entry.name.data(),
					    entry.name.size(),
					    "%.*s (%.*s:%u)",
					    static_cast<int>(function.empty() ? 11 : function.size()),
					    function.empty() ? "<anonymous>" : function.data(),
					    static_cast<int>(script.size()),
					    script.data(),
					    line);
					write(entry.file, script);
				}
				else
				{
					const auto function = rml::luau::access::text(closure.debugname);
					std::snprintf(entry.name.data(),
					    entry.name.size(),
					    "%.*s [C]",
					    static_cast<int>(function.empty() ? 1 : function.size()),
					    function.empty() ? "?" : function.data());
					write(entry.file, "[C]");
				}
				entry.line = line;
				entry.ready.store(true, std::memory_order_release);
				return address_base + index * address_stride;
			}
			if (current == key)
				return address_base + index * address_stride;
		}
		m_dropped.fetch_add(1, std::memory_order_relaxed);
		return 0;
	}

	int LuauFrames::capture(const LuaState& state, std::uint64_t* frames, const int depth)
	{
		int count = 0;
		int visited = 0;
		for (const auto* frame = state.ci; frame && frame > state.base_ci && count < depth && visited < max_visited; --frame, ++visited)
		{
			if (!frame->func || frame->func->tt != LUA_TFUNCTION)
				continue;
			if (const auto address = frame_address(*frame))
				frames[count++] = address;
		}
		return count;
	}

	bool LuauFrames::resolve(const std::uint64_t address, ___tracy_resolved_symbol& symbol) const
	{
		if (address < address_base || address >= address_base + capacity * address_stride)
			return false;
		const auto index = (address - address_base) / address_stride;
		const auto& entry = m_entries[index];
		for (int spin = 0; !entry.ready.load(std::memory_order_acquire); ++spin)
		{
			if (spin == 1000)
				return false;
			std::this_thread::yield();
		}
		symbol = {entry.name.data(), "Luau", entry.file.data(), entry.line, 0, address_base + index * address_stride};
		return true;
	}
}
