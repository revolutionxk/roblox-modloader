#include "capture.hpp"

#include "callstack.hpp"
#include "engine.hpp"
#include "gpu_zones.hpp"
#include "settings.hpp"
#include "timers.hpp"
#include "zone_stack.hpp"

#include <RobloxModLoader/hooking/hooking.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <functional>
#include <thread>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	struct alignas(64) InFlightSlot
	{
		std::atomic<std::int32_t> count{};
	};

	struct SavedGroups
	{
		std::uint32_t force_enable{};
		std::uint32_t all_groups_wanted{};
	};

	static std::array<InFlightSlot, 64> g_in_flight{};
	static std::atomic<bool> g_active{false};
	static bool g_hooked{};
	static bool g_hooked_flip_cpu{};
	static bool g_hooked_put_label{};
	static bool g_hooked_label_literal{};
	static bool g_hooked_on_thread_create{};
	static bool g_hooked_get_token{};
	static std::atomic<bool> g_pass_through{false};
	static std::atomic<bool> g_zone_callstacks{true};
	static std::atomic<std::int32_t> g_callstack_depth{32};
	static SavedGroups g_saved_groups{};
	static MicroProfile* g_state{};
	static Timers* g_timers{};
	static const Callstacks* g_callstacks{};
	static std::atomic<std::uint64_t> g_frame_marks{};
	static std::atomic<std::uint64_t> g_connection_epoch{};
	static std::atomic<bool> g_was_connected{false};
	static std::atomic<GpuZones*> g_gpu_zones{nullptr};
	static thread_local bool t_in_literal = false;
	static constexpr std::size_t max_label_length = 4096;

	static bool observe_connection() noexcept
	{
		if (!___tracy_connected())
		{
			g_was_connected.store(false);
			return false;
		}
		if (!g_was_connected.exchange(true))
			g_connection_epoch.fetch_add(1, std::memory_order_acq_rel);
		return true;
	}

	void attach_gpu_zones(GpuZones* zones)
	{
		g_gpu_zones.store(zones, std::memory_order_release);
	}

	std::uint64_t frame_marks() noexcept
	{
		return g_frame_marks.load(std::memory_order_acquire);
	}

	std::uint64_t connection_epoch() noexcept
	{
		observe_connection();
		return g_connection_epoch.load(std::memory_order_acquire);
	}

	static InFlightSlot& in_flight_slot()
	{
		thread_local auto& slot = g_in_flight[std::hash<std::thread::id>{}(std::this_thread::get_id()) % g_in_flight.size()];
		return slot;
	}

	class Call
	{
	public:
		Call() :
		    m_slot(in_flight_slot())
		{
			m_slot.count.fetch_add(1);
		}

		~Call()
		{
			m_slot.count.fetch_sub(1);
		}

		Call(const Call&) = delete;
		Call& operator=(const Call&) = delete;

	private:
		InFlightSlot& m_slot;
	};

	template<auto detour>
	static auto original()
	{
		static std::atomic<decltype(rml::Hooking::get_original<detour>())> cached{};
		auto function = cached.load(std::memory_order_acquire);
		while (!function)
		{
			function = rml::Hooking::get_original<detour>();
			if (function)
				cached.store(function, std::memory_order_release);
			else
				std::this_thread::yield();
		}
		return function;
	}

	static bool is_gpu(const MicroProfileTimerToken token)
	{
		return (MicroProfileGetGroupMask(token) & std::atomic_ref(g_state->group_mask_gpu).load(std::memory_order_relaxed)) != 0;
	}

	static void begin_zone(const MicroProfileTimerToken token)
	{
		auto& stack = ZoneStack::current();
		if (!___tracy_connected())
		{
			stack.push(token, TracyCZoneCtx{}, nullptr);
			return;
		}

		const auto depth = std::min(g_callstack_depth.load(std::memory_order_relaxed), Callstacks::capacity);
		if (!g_zone_callstacks.load(std::memory_order_relaxed) || depth <= 0)
		{
			const auto* location = g_timers->source_location(token, 0);
			stack.push(token, location ? ___tracy_emit_zone_begin(location, 1) : TracyCZoneCtx{}, location);
			return;
		}

		std::array<std::uint64_t, Callstacks::capacity> frames{};
		const auto captured = g_callstacks->capture(frames.data(), depth);
		const auto caller = captured > 0 ? static_cast<std::uintptr_t>(frames[0]) : 0;
		const auto* location = g_timers->source_location(token, caller);
		if (!location)
		{
			stack.push(token, TracyCZoneCtx{}, nullptr);
			return;
		}

		stack.push(token, ___tracy_emit_zone_begin_frames(location, frames.data(), captured, 1), location);
	}

	static std::uint64_t enter(const MicroProfileTimerToken token, const std::uint64_t tick)
	{
		const Call call;
		if (!g_active.load())
			return original<&enter>()(token, tick);
		if (is_gpu(token))
		{
			const auto result = original<&enter>()(token, tick);
			bool opened = false;
			if (auto* zones = g_gpu_zones.load(std::memory_order_acquire))
			{
				try
				{
					opened = zones->enter(token);
				}
				catch (...)
				{
				}
			}
			return opened && result == MICROPROFILE_INVALID_TICK ? 0 : result;
		}

		const auto result = g_pass_through.load(std::memory_order_relaxed) ? original<&enter>()(token, tick) : (tick == MICROPROFILE_INVALID_TICK ? 0 : tick);
		if (result == MICROPROFILE_INVALID_TICK)
			return result;

		try
		{
			begin_zone(token);
		}
		catch (...)
		{
			ZoneStack::current().push(token, TracyCZoneCtx{}, nullptr);
		}
		return result;
	}

	static void leave(const MicroProfileTimerToken token, const std::uint64_t enter_tick, const std::uint64_t leave_tick)
	{
		const Call call;
		if (!g_active.load())
			return original<&leave>()(token, enter_tick, leave_tick);
		if (is_gpu(token))
		{
			if (auto* zones = g_gpu_zones.load(std::memory_order_acquire))
			{
				try
				{
					zones->leave(token);
				}
				catch (...)
				{
				}
			}
			if (enter_tick == 0)
				return;
			return original<&leave>()(token, enter_tick, leave_tick);
		}

		if (g_pass_through.load(std::memory_order_relaxed))
			original<&leave>()(token, enter_tick, leave_tick);

		if (enter_tick == MICROPROFILE_INVALID_TICK)
			return;

		ZoneStack::current().pop(token);
	}

	static void emit_health()
	{
		auto& health = ZoneStack::health();
		___tracy_emit_plot_int("RML/Unbalanced leaves", health.unbalanced.load(std::memory_order_relaxed));
		___tracy_emit_plot_int("RML/Orphaned leaves", health.orphaned.load(std::memory_order_relaxed));
		___tracy_emit_plot_int("RML/Stack overflows", health.overflowed.load(std::memory_order_relaxed));
	}

	static void flip_cpu()
	{
		const Call call;
		original<&flip_cpu>()();
		if (!g_active.load())
			return;

		const bool connected = observe_connection();

		___tracy_emit_frame_mark(nullptr);
		g_frame_marks.fetch_add(1, std::memory_order_acq_rel);
		if (!connected)
			return;

		try
		{
			g_timers->emit_counters();
			emit_health();
			if (auto* zones = g_gpu_zones.load(std::memory_order_acquire))
				zones->on_frame();
		}
		catch (...)
		{
		}
	}

	static void emit_label(const char* text)
	{
		if (!text || !___tracy_connected())
			return;

		const auto length = strnlen(text, max_label_length);
		auto& stack = ZoneStack::current();
		const auto* entry = stack.innermost();
		if (!entry || !entry->zone.active)
		{
			___tracy_emit_logString(TracyMessageSeverityInfo, 0, 0, length, text);
			return;
		}

		___tracy_emit_zone_text(entry->zone, text, length);
		try
		{
			const auto name = stack.append_label(*entry, std::string_view(text, length));
			___tracy_emit_zone_name(entry->zone, name.data(), name.size());
		}
		catch (...)
		{
		}
	}

	static void put_label(const MicroProfileLabelToken label, const char* text)
	{
		const Call call;
		original<&put_label>()(label, text);
		if (g_active.load() && !t_in_literal)
			emit_label(text);
	}

	static void label_literal(const MicroProfileLabelToken label, const char* text)
	{
		const Call call;
		t_in_literal = true;
		original<&label_literal>()(label, text);
		t_in_literal = false;
		if (g_active.load())
			emit_label(text);
	}

	static void on_thread_create(const char* name, const RBX::ThreadBufferSizeType buffer_size)
	{
		const Call call;
		original<&on_thread_create>()(name, buffer_size);
		if (g_active.load() && name)
			___tracy_set_thread_name(name);
	}

	static MicroProfileTimerToken get_token(const char* group, const char* name, const int color, const std::uint8_t flags)
	{
		const Call call;
		const auto token = original<&get_token>()(group, name, color, flags);
		if (!g_active.load())
			return token;

		try
		{
			g_timers->record_owner(token, g_callstacks->studio_caller());
		}
		catch (...)
		{
		}
		return token;
	}

	static void force_groups()
	{
		g_saved_groups = {g_state->force_enable, g_state->all_groups_wanted};
		g_state->force_enable = 1;
		g_state->all_groups_wanted = 1;
	}

	static void restore_groups()
	{
		g_state->force_enable = g_saved_groups.force_enable;
		g_state->all_groups_wanted = g_saved_groups.all_groups_wanted;
	}

	static void wait_idle()
	{
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
		for (const auto& slot : g_in_flight)
		{
			while (slot.count.load() != 0 && std::chrono::steady_clock::now() < deadline)
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

	void apply_settings(const Settings& settings)
	{
		g_pass_through.store(settings.pass_through, std::memory_order_relaxed);
		g_zone_callstacks.store(settings.zone_callstacks, std::memory_order_relaxed);
		g_callstack_depth.store(settings.callstack_depth, std::memory_order_relaxed);
	}

	void install_capture(const Engine& engine, Timers& timers, const Callstacks& callstacks)
	{
		g_state = engine.state;
		g_timers = &timers;
		g_callstacks = &callstacks;

		rml::Hooking::DetourHookHelper::add<&enter>("MicroProfileEnter", reinterpret_cast<void*>(engine.enter));
		rml::Hooking::DetourHookHelper::add<&leave>("MicroProfileLeave", reinterpret_cast<void*>(engine.leave));
		g_hooked = true;

		if (engine.flip_cpu)
		{
			rml::Hooking::DetourHookHelper::add<&flip_cpu>("MicroProfileFlipCpu", reinterpret_cast<void*>(engine.flip_cpu));
			g_hooked_flip_cpu = true;
		}
		if (engine.put_label)
		{
			rml::Hooking::DetourHookHelper::add<&put_label>("MicroProfilePutLabel", reinterpret_cast<void*>(engine.put_label));
			g_hooked_put_label = true;
		}
		if (engine.label_literal)
		{
			rml::Hooking::DetourHookHelper::add<&label_literal>("MicroProfileLabelLiteral",
			    reinterpret_cast<void*>(engine.label_literal));
			g_hooked_label_literal = true;
		}
		if (engine.on_thread_create)
		{
			rml::Hooking::DetourHookHelper::add<&on_thread_create>("MicroProfileOnThreadCreate",
			    reinterpret_cast<void*>(engine.on_thread_create));
			g_hooked_on_thread_create = true;
		}
		if (engine.get_token)
		{
			rml::Hooking::DetourHookHelper::add<&get_token>("ProfilerGetToken", reinterpret_cast<void*>(engine.get_token));
			g_hooked_get_token = true;
		}

		g_active.store(true);
		force_groups();
	}

	void remove_capture()
	{
		if (!g_hooked)
			return;

		g_active.store(false);

		rml::Hooking::DetourHookHelper::disable<&enter>();
		rml::Hooking::DetourHookHelper::disable<&leave>();
		if (g_hooked_flip_cpu)
			rml::Hooking::DetourHookHelper::disable<&flip_cpu>();
		if (g_hooked_put_label)
			rml::Hooking::DetourHookHelper::disable<&put_label>();
		if (g_hooked_label_literal)
			rml::Hooking::DetourHookHelper::disable<&label_literal>();
		if (g_hooked_on_thread_create)
			rml::Hooking::DetourHookHelper::disable<&on_thread_create>();
		if (g_hooked_get_token)
			rml::Hooking::DetourHookHelper::disable<&get_token>();

		wait_idle();

		rml::Hooking::DetourHookHelper::remove<&enter>();
		rml::Hooking::DetourHookHelper::remove<&leave>();
		if (g_hooked_flip_cpu)
			rml::Hooking::DetourHookHelper::remove<&flip_cpu>();
		if (g_hooked_put_label)
			rml::Hooking::DetourHookHelper::remove<&put_label>();
		if (g_hooked_label_literal)
			rml::Hooking::DetourHookHelper::remove<&label_literal>();
		if (g_hooked_on_thread_create)
			rml::Hooking::DetourHookHelper::remove<&on_thread_create>();
		if (g_hooked_get_token)
			rml::Hooking::DetourHookHelper::remove<&get_token>();

		restore_groups();
		g_hooked = false;
		g_hooked_flip_cpu = false;
		g_hooked_put_label = false;
		g_hooked_label_literal = false;
		g_hooked_on_thread_create = false;
		g_hooked_get_token = false;

		g_state = nullptr;
		g_timers = nullptr;
		g_callstacks = nullptr;
	}
}
