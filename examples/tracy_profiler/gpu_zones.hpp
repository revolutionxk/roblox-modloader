#pragma once

#include <RobloxModLoader/platform/graphics/gpu_timeline.hpp>
#include <RobloxModLoader/roblox/profiler/micro_profile.hpp>
#include <atomic>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	class Timers;
	class Callstacks;

	class GpuZones
	{
	public:
		GpuZones(Timers& timers, const Callstacks& callstacks);
		~GpuZones();

		GpuZones(const GpuZones&) = delete;
		GpuZones& operator=(const GpuZones&) = delete;

		void stop();
		void set_enabled(bool enabled);
		[[nodiscard]] bool running() const noexcept;
		bool enter(MicroProfileTimerToken token);
		void leave(MicroProfileTimerToken token);
		void on_frame();

	private:
		struct Encoder
		{
			std::uint64_t thread;
			std::uint64_t begin;
			std::uint64_t end;
		};

		struct Scope
		{
			std::uint64_t thread;
			std::uint64_t start;
			std::uint64_t last;
			std::uint64_t parent_start;
			std::uint16_t begin_query;
			std::uint16_t end_query;
			std::uint64_t frame;
		};

		static void sink(const rml::platform::GpuCommandBufferTiming& timing, void* user);
		static void emit_times(const Scope& scope, std::int64_t begin, std::int64_t end);
		void deliver(const rml::platform::GpuCommandBufferTiming& timing);
		bool start_locked();
		void stop_locked();
		void create_contexts();
		void sync_connection(std::uint64_t epoch);
		std::uint16_t next_query();
		std::int64_t fallback_time() const;
		bool try_resolve(const Scope& scope);
		void prune();

		Timers& m_timers;
		const Callstacks& m_callstacks;
		std::atomic<bool> m_started{false};
		std::atomic<bool> m_enabled{true};
		std::atomic<std::uint64_t> m_context_epoch{0};
		std::atomic<std::uint32_t> m_query{0};
		std::atomic<std::int64_t> m_dropped{0};
		std::uint64_t m_frames{};
		bool m_contexts_created{};
		std::mutex m_control;
		std::mutex m_mutex;
		std::map<std::uint64_t, Encoder> m_encoders;
		std::deque<Scope> m_pending;
		___tracy_source_location_data m_command_buffer_location{};
	};
}
