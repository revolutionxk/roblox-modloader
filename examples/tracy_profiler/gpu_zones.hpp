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

		[[nodiscard]] bool start();
		void stop();
		void set_enabled(bool enabled);
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
			std::uint16_t begin_query;
			std::uint16_t end_query;
			std::uint64_t frame;
		};

		static void sink(const rml::platform::GpuCommandBufferTiming& timing, void* user);
		void deliver(const rml::platform::GpuCommandBufferTiming& timing);
		bool ensure_context();
		std::uint16_t next_query();
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
		bool m_context_created{};
		std::mutex m_mutex;
		std::map<std::uint64_t, Encoder> m_encoders;
		std::deque<Scope> m_pending;
		___tracy_source_location_data m_command_buffer_location{};
	};
}
