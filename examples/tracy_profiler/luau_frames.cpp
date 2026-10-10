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

	static constexpr std::uint64_t fnv_basis = 0xCBF29CE484222325ULL;
	static constexpr std::uint64_t fnv_prime = 0x100000001B3ULL;
	static constexpr std::uint64_t native_flag = 0xC;

	static std::uint64_t mix(std::uint64_t value)
	{
		value ^= value >> 33;
		value *= 0xFF51AFD7ED558CCDULL;
		value ^= value >> 33;
		value *= 0xC4CEB9FE1A85EC53ULL;
		value ^= value >> 33;
		return value;
	}

	static std::uint64_t fold(std::uint64_t hash, const std::string_view text)
	{
		for (const auto byte : text)
			hash = (hash ^ static_cast<std::uint8_t>(byte)) * fnv_prime;
		return (hash ^ text.size()) * fnv_prime;
	}

	static std::uint64_t lua_key(const Proto& proto)
	{
		auto hash = fold(fnv_basis, rml::luau::access::text(proto.source));
		hash = fold(hash, rml::luau::access::text(proto.debugname));
		return mix((hash ^ static_cast<std::uint32_t>(proto.linedefined)) * fnv_prime) | 1;
	}

	static std::uint64_t native_key(const Closure& closure)
	{
		return mix((fold(fnv_basis, rml::luau::access::text(closure.debugname)) ^ native_flag) * fnv_prime) | 1;
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

	void LuauFrames::publish(Entry& entry, const Closure& closure, const Proto* proto)
	{
		entry.native = proto == nullptr;
		if (proto)
		{
			const auto function = rml::luau::access::text(proto->debugname);
			write(entry.name, function.empty() ? std::string_view{"<anonymous>"} : function);
			write(entry.script, script_of(rml::luau::access::text(proto->source)));
		}
		else
		{
			const auto function = rml::luau::access::text(closure.debugname);
			write(entry.name, function.empty() ? std::string_view{"?"} : function);
			write(entry.script, "[C]");
		}
		entry.ready.store(true, std::memory_order_release);
	}

	std::uint32_t LuauFrames::intern(const Closure& closure, const Proto* proto)
	{
		const auto key = proto ? lua_key(*proto) : native_key(closure);
		auto index = (key >> 1) & (capacity - 1);
		for (std::size_t probe = 0; probe < probe_limit; ++probe, index = (index + 1) & (capacity - 1))
		{
			auto& entry = m_entries[index];
			auto current = entry.key.load(std::memory_order_acquire);
			if (current == 0 && entry.key.compare_exchange_strong(current, key, std::memory_order_acq_rel))
			{
				publish(entry, closure, proto);
				return static_cast<std::uint32_t>(index);
			}
			if (current == key)
				return static_cast<std::uint32_t>(index);
		}
		return no_index;
	}

	std::uint64_t LuauFrames::frame_address(const CallInfo& frame)
	{
		thread_local std::array<CacheSlot, cache_size> cache;

		const auto& closure = *rml::luau::access::closure_in(frame.func);
		const Proto* proto = closure.isC ? nullptr : closure.p;
		const void* owner = closure.p;
		const void* source = proto ? static_cast<const void*>(proto->source) : static_cast<const void*>(closure.debugname);
		const auto linedefined = proto ? proto->linedefined : 0;

		auto& slot = cache[(reinterpret_cast<std::uintptr_t>(owner) * 0x9E3779B97F4A7C15ULL) >> (64 - cache_bits)];
		if (slot.index == no_index || slot.proto != owner || slot.source != source || slot.linedefined != linedefined)
		{
			const auto index = intern(closure, proto);
			if (index == no_index)
			{
				m_dropped.fetch_add(1, std::memory_order_relaxed);
				return 0;
			}
			slot = {owner, source, linedefined, index};
		}
		const auto line = proto ? current_line(frame, *proto) : 0;
		return address_base + (std::uint64_t{slot.index} << line_bits) + std::min(line, line_mask);
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
		if (address < address_base || address - address_base >= capacity << line_bits)
			return false;
		const auto index = (address - address_base) >> line_bits;
		const auto line = static_cast<std::uint32_t>(address & line_mask);
		const auto& entry = m_entries[index];
		for (int spin = 0; !entry.ready.load(std::memory_order_acquire); ++spin)
		{
			if (spin == 1000)
				return false;
			std::this_thread::yield();
		}
		thread_local std::array<char, 256> buffer;
		if (entry.native)
			std::snprintf(buffer.data(), buffer.size(), "%s [C]", entry.name.data());
		else
			std::snprintf(buffer.data(), buffer.size(), "%s (%s:%u)", entry.name.data(), entry.script.data(), line);
		symbol = {buffer.data(), "Luau", entry.script.data(), line, 0, address};
		return true;
	}
}
