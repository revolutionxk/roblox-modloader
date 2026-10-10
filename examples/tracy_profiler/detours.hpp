#pragma once

#include <RobloxModLoader/hooking/hooking.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace tracy_profiler
{
	class InFlight
	{
	public:
		class Call
		{
		public:
			explicit Call(InFlight& in_flight);
			~Call();

			Call(const Call&) = delete;
			Call& operator=(const Call&) = delete;

		private:
			std::atomic<std::int32_t>& m_count;
		};

		void wait_idle(std::chrono::milliseconds timeout) const;

	private:
		struct alignas(64) Slot
		{
			std::atomic<std::int32_t> count{};
		};

		std::array<Slot, 64> m_slots{};
	};

	template<auto detour>
	auto& cached_original()
	{
		static std::atomic<decltype(rml::Hooking::get_original<detour>())> cached{};
		return cached;
	}

	template<auto detour>
	auto original()
	{
		auto& cached = cached_original<detour>();
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

	template<auto detour>
	void forget_original()
	{
		cached_original<detour>().store(nullptr, std::memory_order_release);
	}
}
