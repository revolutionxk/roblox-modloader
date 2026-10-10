#include "gpu_zones.hpp"

#include "callstack.hpp"
#include "capture.hpp"
#include "timers.hpp"

#include <RobloxModLoader/platform/debug/call_stack.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace tracy_profiler
{
	static constexpr std::uint8_t context_id = 0;
	static constexpr std::uint64_t timeout_frames = 8;
	static constexpr std::size_t encoder_window = 32768;
	static constexpr int gpu_callstack_depth = 16;

	struct OpenScope
	{
		MicroProfileTimerToken token;
		std::uint64_t start;
		std::uint16_t query;
		std::uint64_t epoch;
		bool active;
	};

	static thread_local std::vector<OpenScope> t_open;

	static std::uint8_t tracy_type(const std::string_view api)
	{
		if (api == "Metal")
			return 6;
		if (api == "Direct3D 11")
			return 5;
		if (api == "Vulkan")
			return 2;
		return 7;
	}

	GpuZones::GpuZones(Timers& timers, const Callstacks& callstacks) :
	    m_timers(timers),
	    m_callstacks(callstacks)
	{
		m_command_buffer_location = {"Command buffer", "GPU", rml::platform::gpu_timeline_api().data(), 0, 0x808080};
	}

	GpuZones::~GpuZones()
	{
		stop();
	}

	bool GpuZones::start()
	{
		if (m_started.load())
			return true;
		m_started.store(rml::platform::start_gpu_timeline(&GpuZones::sink, this));
		return m_started.load();
	}

	void GpuZones::stop()
	{
		if (!m_started.exchange(false))
			return;
		rml::platform::stop_gpu_timeline();
		std::scoped_lock lock(m_mutex);
		m_pending.clear();
		m_encoders.clear();
	}

	void GpuZones::set_enabled(const bool enabled)
	{
		m_enabled.store(enabled);
	}

	std::uint16_t GpuZones::next_query()
	{
		return static_cast<std::uint16_t>(m_query.fetch_add(1));
	}

	bool GpuZones::ensure_context()
	{
		const auto epoch = connection_epoch();
		if (m_context_epoch.load() == epoch)
			return true;

		std::scoped_lock lock(m_mutex);
		if (m_context_epoch.load() == epoch)
			return true;

		m_pending.clear();
		m_encoders.clear();
		if (!m_context_created)
		{
			const auto api = rml::platform::gpu_timeline_api();
			___tracy_emit_gpu_new_context_serial({static_cast<std::int64_t>(rml::platform::gpu_timeline_now()), static_cast<float>(rml::platform::gpu_timeline_period()), context_id, 0, tracy_type(api)});
			___tracy_emit_gpu_context_name_serial({context_id, api.data(), static_cast<std::uint16_t>(api.size())});
			m_context_created = true;
		}
		m_context_epoch.store(epoch);
		return true;
	}

	bool GpuZones::enter(const MicroProfileTimerToken token)
	{
		if (!m_started.load() || !m_enabled.load() || !___tracy_connected() || !ensure_context())
		{
			t_open.push_back({token, 0, 0, 0, false});
			return false;
		}

		std::array<std::uint64_t, Callstacks::capacity> frames{};
		const auto depth = m_callstacks.capture(frames.data(), gpu_callstack_depth);
		const auto caller = depth > 0 ? static_cast<std::uintptr_t>(frames[0]) : 0;
		const auto* location = m_timers.source_location(token, caller);
		if (!location)
		{
			t_open.push_back({token, 0, 0, 0, false});
			return false;
		}

		const auto query = next_query();
		___tracy_emit_gpu_zone_begin_frames_serial({reinterpret_cast<std::uint64_t>(location), query, context_id},
		    frames.data(),
		    depth);
		t_open.push_back({token, rml::platform::gpu_timeline_serial(), query, m_context_epoch.load(), true});
		return true;
	}

	void GpuZones::leave(const MicroProfileTimerToken token)
	{
		const auto match = std::find_if(t_open.rbegin(), t_open.rend(), [token](const OpenScope& open) {
			return open.token == token;
		});
		if (match == t_open.rend())
			return;

		const auto last = rml::platform::gpu_timeline_serial();
		const auto thread = rml::platform::current_thread_id();
		const auto epoch = m_context_epoch.load();
		const auto connected = ___tracy_connected() != 0;
		const auto keep = static_cast<std::size_t>(std::distance(match, t_open.rend()) - 1);
		while (t_open.size() > keep)
		{
			const auto open = t_open.back();
			t_open.pop_back();
			if (!open.active || open.epoch != epoch || !connected)
				continue;

			const auto end_query = next_query();
			___tracy_emit_gpu_zone_end_serial({end_query, context_id});
			std::scoped_lock lock(m_mutex);
			m_pending.push_back({thread, open.start, last, open.query, end_query, m_frames});
		}
	}

	void GpuZones::sink(const rml::platform::GpuCommandBufferTiming& timing, void* user)
	{
		static_cast<GpuZones*>(user)->deliver(timing);
	}

	bool GpuZones::try_resolve(const Scope& scope)
	{
		if (scope.last <= scope.start)
		{
			const auto next = m_encoders.lower_bound(scope.start + 1);
			if (next == m_encoders.end())
				return false;
			___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(next->second.begin), scope.begin_query, context_id});
			___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(next->second.begin), scope.end_query, context_id});
			return true;
		}

		std::uint64_t begin = UINT64_MAX;
		std::uint64_t end = 0;
		for (auto serial = scope.start + 1; serial <= scope.last; ++serial)
		{
			const auto it = m_encoders.find(serial);
			if (it == m_encoders.end())
				return false;
			if (it->second.thread != scope.thread)
				continue;
			begin = std::min(begin, it->second.begin);
			end = std::max(end, it->second.end);
		}
		if (begin == UINT64_MAX)
		{
			const auto next = m_encoders.upper_bound(scope.last);
			if (next == m_encoders.end())
				return false;
			begin = end = next->second.begin;
		}
		___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(begin), scope.begin_query, context_id});
		___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(end), scope.end_query, context_id});
		return true;
	}

	void GpuZones::prune()
	{
		while (m_encoders.size() > encoder_window)
			m_encoders.erase(m_encoders.begin());
	}

	void GpuZones::deliver(const rml::platform::GpuCommandBufferTiming& timing)
	{
		if (!m_started.load() || !___tracy_connected() || m_context_epoch.load() != connection_epoch())
			return;

		std::scoped_lock lock(m_mutex);
		for (const auto& encoder : timing.encoders)
			m_encoders.insert_or_assign(encoder.serial, Encoder{encoder.thread, encoder.begin, encoder.end});

		if (!timing.encoders.empty() && timing.end > timing.begin)
		{
			const auto begin_query = next_query();
			const auto end_query = next_query();
			___tracy_emit_gpu_zone_begin_serial({reinterpret_cast<std::uint64_t>(&m_command_buffer_location), begin_query, context_id});
			___tracy_emit_gpu_zone_end_serial({end_query, context_id});
			___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(timing.begin), begin_query, context_id});
			___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(timing.end), end_query, context_id});
		}

		std::erase_if(m_pending, [this](const Scope& scope) {
			return try_resolve(scope);
		});
		prune();
	}

	void GpuZones::on_frame()
	{
		std::scoped_lock lock(m_mutex);
		++m_frames;
		const auto fallback = m_encoders.empty() ? static_cast<std::int64_t>(rml::platform::gpu_timeline_now()) :
		                                           static_cast<std::int64_t>(m_encoders.rbegin()->second.end);
		std::erase_if(m_pending, [&](const Scope& scope) {
			if (m_frames < scope.frame + timeout_frames)
				return false;
			___tracy_emit_gpu_time_serial({fallback, scope.begin_query, context_id});
			___tracy_emit_gpu_time_serial({fallback, scope.end_query, context_id});
			m_dropped.fetch_add(1);
			return true;
		});
		___tracy_emit_plot_int("RML/GPU dropped", m_dropped.load());
	}
}
