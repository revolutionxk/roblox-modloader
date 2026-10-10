#include "gpu_zones.hpp"

#include "callstack.hpp"
#include "capture.hpp"
#include "timers.hpp"

#include <RobloxModLoader/platform/debug/call_stack.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace tracy_profiler
{
	static constexpr std::uint8_t scope_context = 0;
	static constexpr std::uint8_t buffer_context = 1;
	static constexpr std::uint64_t no_parent = UINT64_MAX;
	static constexpr std::uint64_t timeout_frames = 8;
	static constexpr std::size_t encoder_window = 32768;
	static constexpr std::uint64_t anchor_walk = 64;
	static constexpr int gpu_callstack_depth = 16;

	struct OpenScope
	{
		MicroProfileTimerToken token;
		std::uint64_t start;
		std::uint16_t query;
		std::uint64_t epoch;
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

	bool GpuZones::start_locked()
	{
		if (m_started.load())
			return true;
		if (!rml::platform::start_gpu_timeline(&GpuZones::sink, this))
			return false;
		create_contexts();
		m_started.store(true);
		return true;
	}

	void GpuZones::stop_locked()
	{
		if (!m_started.exchange(false))
			return;
		rml::platform::stop_gpu_timeline();
		std::scoped_lock lock(m_mutex);
		if (___tracy_connected() && m_context_epoch.load() == connection_epoch())
		{
			const auto fallback = fallback_time();
			for (const auto& scope : m_pending)
				settle(scope, fallback, fallback);
		}
		m_pending.clear();
		m_settled.clear();
		m_encoders.clear();
	}

	void GpuZones::stop()
	{
		std::scoped_lock control(m_control);
		stop_locked();
	}

	void GpuZones::set_enabled(const bool enabled)
	{
		std::scoped_lock control(m_control);
		m_enabled.store(enabled);
		if (enabled)
			(void)start_locked();
		else
			stop_locked();
	}

	bool GpuZones::running() const noexcept
	{
		return m_started.load();
	}

	void GpuZones::create_contexts()
	{
		if (m_contexts_created)
			return;

		const auto api = rml::platform::gpu_timeline_api();
		const std::string buffers = std::string(api) + " command buffers";
		const auto now = static_cast<std::int64_t>(rml::platform::gpu_timeline_now());
		const auto period = static_cast<float>(rml::platform::gpu_timeline_period());
		const auto type = tracy_type(api);
		___tracy_emit_gpu_new_context_serial({now, period, scope_context, 0, type});
		___tracy_emit_gpu_context_name_serial({scope_context, api.data(), static_cast<std::uint16_t>(api.size())});
		___tracy_emit_gpu_new_context_serial({now, period, buffer_context, 0, type});
		___tracy_emit_gpu_context_name_serial({buffer_context, buffers.data(), static_cast<std::uint16_t>(buffers.size())});
		m_contexts_created = true;
	}

	std::uint16_t GpuZones::next_query()
	{
		return static_cast<std::uint16_t>(m_query.fetch_add(1));
	}

	void GpuZones::sync_connection(const std::uint64_t epoch)
	{
		if (m_context_epoch.load() == epoch)
			return;

		std::scoped_lock lock(m_mutex);
		if (m_context_epoch.load() == epoch)
			return;

		m_pending.clear();
		m_settled.clear();
		m_encoders.clear();
		m_context_epoch.store(epoch);
	}

	std::int64_t GpuZones::fallback_time() const
	{
		if (m_encoders.empty())
			return static_cast<std::int64_t>(rml::platform::gpu_timeline_now());
		return static_cast<std::int64_t>(m_encoders.rbegin()->second.end);
	}

	void GpuZones::settle(const Scope& scope, const std::int64_t begin, const std::int64_t end)
	{
		___tracy_emit_gpu_time_serial({begin, scope.begin_query, scope_context});
		___tracy_emit_gpu_time_serial({end, scope.end_query, scope_context});
		m_settled.insert_or_assign(scope.begin_query, Settled{scope.start, begin, m_frames});
	}

	bool GpuZones::enter(const MicroProfileTimerToken token)
	{
		if (!m_started.load() || !m_enabled.load() || !___tracy_connected())
			return false;

		const auto epoch = connection_epoch();
		sync_connection(epoch);

		std::array<std::uint64_t, Callstacks::capacity> frames{};
		const auto depth = m_callstacks.capture(frames.data(), gpu_callstack_depth);
		const auto caller = depth > 0 ? static_cast<std::uintptr_t>(frames[0]) : 0;
		const auto* location = m_timers.source_location(token, caller);
		if (!location)
			return false;

		const auto query = next_query();
		___tracy_emit_gpu_zone_begin_frames_serial({reinterpret_cast<std::uint64_t>(location), query, scope_context},
		    frames.data(),
		    depth);
		t_open.push_back({token, rml::platform::gpu_timeline_serial(), query, epoch});
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
		const auto epoch = connection_epoch();
		const auto connected = ___tracy_connected() != 0;
		const auto keep = static_cast<std::size_t>(std::distance(match, t_open.rend()) - 1);
		while (t_open.size() > keep)
		{
			const auto open = t_open.back();
			t_open.pop_back();
			if (open.epoch != epoch || !connected)
				continue;

			const auto has_parent = !t_open.empty() && t_open.back().epoch == epoch;
			const auto parent_start = has_parent ? t_open.back().start : no_parent;
			const auto parent_query = has_parent ? t_open.back().query : std::uint16_t{};
			const auto end_query = next_query();
			___tracy_emit_gpu_zone_end_serial({end_query, scope_context});
			std::scoped_lock lock(m_mutex);
			if (m_context_epoch.load() != open.epoch)
				continue;

			const Scope scope{thread, open.start, last, parent_start, parent_query, open.query, end_query, m_frames};
			if (try_resolve(scope))
				resolve_pending();
			else
				m_pending.push_back(scope);
		}
	}

	void GpuZones::sink(const rml::platform::GpuCommandBufferTiming& timing, void* user)
	{
		static_cast<GpuZones*>(user)->deliver(timing);
	}

	bool GpuZones::try_resolve(const Scope& scope)
	{
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
		if (begin != UINT64_MAX)
		{
			settle(scope, static_cast<std::int64_t>(begin), static_cast<std::int64_t>(end));
			return true;
		}

		if (scope.parent_start != no_parent && scope.start <= scope.parent_start)
		{
			const auto parent = m_settled.find(scope.parent_query);
			if (parent == m_settled.end() || parent->second.start != scope.parent_start)
				return false;
			settle(scope, parent->second.begin, parent->second.begin);
			return true;
		}

		const auto floor = scope.parent_start == no_parent ? 0 : scope.parent_start;
		const auto lowest = scope.start > anchor_walk ? std::max(floor, scope.start - anchor_walk) : floor;
		for (auto serial = scope.start; serial > lowest; --serial)
		{
			const auto it = m_encoders.find(serial);
			if (it == m_encoders.end())
				return false;
			if (it->second.thread != scope.thread)
				continue;
			settle(scope, static_cast<std::int64_t>(it->second.end), static_cast<std::int64_t>(it->second.end));
			return true;
		}

		const auto next = m_encoders.upper_bound(scope.last);
		if (next == m_encoders.end())
			return false;
		settle(scope, static_cast<std::int64_t>(next->second.begin), static_cast<std::int64_t>(next->second.begin));
		return true;
	}

	void GpuZones::resolve_pending()
	{
		bool progress = true;
		while (progress)
		{
			progress = false;
			std::erase_if(m_pending, [&](const Scope& scope) {
				if (!try_resolve(scope))
					return false;
				progress = true;
				return true;
			});
		}
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
			___tracy_emit_gpu_zone_begin_serial({reinterpret_cast<std::uint64_t>(&m_command_buffer_location), begin_query, buffer_context});
			___tracy_emit_gpu_zone_end_serial({end_query, buffer_context});
			___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(timing.begin), begin_query, buffer_context});
			___tracy_emit_gpu_time_serial({static_cast<std::int64_t>(timing.end), end_query, buffer_context});
		}

		resolve_pending();
		prune();
	}

	void GpuZones::on_frame()
	{
		std::scoped_lock lock(m_mutex);
		if (m_context_epoch.load() != connection_epoch())
			return;

		++m_frames;
		const auto fallback = fallback_time();
		std::erase_if(m_pending, [&](const Scope& scope) {
			if (m_frames < scope.frame + timeout_frames)
				return false;
			settle(scope, fallback, fallback);
			if (scope.last > scope.start)
				m_dropped.fetch_add(1);
			return true;
		});
		std::erase_if(m_settled, [this](const auto& entry) {
			return m_frames > entry.second.frame + timeout_frames;
		});
		___tracy_emit_plot_int("RML/GPU dropped", m_dropped.load());
	}
}
