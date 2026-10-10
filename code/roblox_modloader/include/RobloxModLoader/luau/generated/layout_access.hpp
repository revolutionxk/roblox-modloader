#pragma once

#include "RobloxModLoader/luau/generated/luau_layout.hpp"

#include <cstring>
#include <string_view>

namespace rml::luau::access
{
	using namespace mirror;

	[[nodiscard]] inline LuaState* state(void* thread) { return static_cast<LuaState*>(thread); }
	[[nodiscard]] inline Closure* closure(void* function) { return static_cast<Closure*>(function); }
	[[nodiscard]] inline Proto* proto(void* prototype) { return static_cast<Proto*>(prototype); }
	[[nodiscard]] inline TValue* value(void* slot) { return static_cast<TValue*>(slot); }
	[[nodiscard]] inline CallInfo* frame(void* info) { return static_cast<CallInfo*>(info); }
	[[nodiscard]] inline LuaDebug* debug_record(void* storage) { return static_cast<LuaDebug*>(storage); }
	[[nodiscard]] inline const GcHeader* gc_header(const void* object)
	{
		return static_cast<const GcHeader*>(object);
	}

	static_assert(sizeof(TValue) == tvalue_size);
	static_assert(sizeof(CallInfo) == callinfo_size);

	inline constexpr std::uint8_t white_bits = 3;
	inline constexpr std::uint8_t black_bit = 4;

	[[nodiscard]] inline bool swept_away(const GlobalState* collector, const void* object)
	{
		const auto other = static_cast<std::uint8_t>(collector->currentwhite ^ white_bits) & white_bits;

		return (gc_header(object)->marked & other) != 0;
	}

	[[nodiscard]] inline bool needs_barrier(const void* owner, const void* value)
	{
		return (gc_header(owner)->marked & black_bit) != 0 && (gc_header(value)->marked & white_bits) != 0;
	}

	[[nodiscard]] inline const Closure* closure(const void* function)
	{
		return static_cast<const Closure*>(function);
	}

	[[nodiscard]] inline const Proto* proto(const void* prototype)
	{
		return static_cast<const Proto*>(prototype);
	}

	[[nodiscard]] inline const TValue* value(const void* slot) { return static_cast<const TValue*>(slot); }

	[[nodiscard]] inline const TValue* value_at(const TValue* first, const int index)
	{
		return first + index;
	}

	[[nodiscard]] inline TValue* value_at(TValue* first, const int index)
	{
		return first + index;
	}

	[[nodiscard]] inline CallInfo* frame_at(CallInfo* first, const int index)
	{
		return first + index;
	}

	[[nodiscard]] inline std::ptrdiff_t frames_between(const CallInfo* from, const CallInfo* to)
	{
		return from - to;
	}

	[[nodiscard]] inline Closure* closure_in(const TValue* slot)
	{
		return *reinterpret_cast<Closure* const*>(&slot->value);
	}

	[[nodiscard]] inline TValue* upvalues_of(const Closure* function)
	{
		const auto start = function->isC != 0 ? offsetof(Closure, upvals) : offsetof(Closure, uprefs);
		auto* bytes = reinterpret_cast<std::byte*>(const_cast<Closure*>(function));

		return reinterpret_cast<TValue*>(bytes + start);
	}

	inline void copy_value(TValue* destination, const TValue* source)
	{
		*destination = *source;
	}

	inline void advance_top(LuaState* thread)
	{
		thread->top = value_at(thread->top, 1);
	}

	[[nodiscard]] inline std::string_view text(const TString* string)
	{
		return string != nullptr ? std::string_view{string->data, string->len} : std::string_view{};
	}
}
