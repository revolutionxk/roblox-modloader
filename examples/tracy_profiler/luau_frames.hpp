#pragma once

#include <RobloxModLoader/luau/generated/luau_layout.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <tracy/TracyC.h>

namespace tracy_profiler
{
	class LuauFrames
	{
	public:
		static constexpr int max_depth = 32;
		static constexpr int max_visited = 256;

		LuauFrames();

		[[nodiscard]] int capture(const rml::luau::mirror::LuaState& state, std::uint64_t* frames, int depth);
		[[nodiscard]] bool resolve(std::uint64_t address, ___tracy_resolved_symbol& symbol) const;
		[[nodiscard]] std::uint64_t dropped() const;

	private:
		struct Entry
		{
			std::atomic<std::uint64_t> key{};
			std::atomic<bool> ready{};
			std::uint32_t line{};
			std::array<char, 96> name{};
			std::array<char, 160> file{};
		};

		static constexpr std::size_t capacity = std::size_t{1} << 14;
		static constexpr std::size_t probe_limit = 32;
		static constexpr std::uint64_t address_base = 0x0050'0000'0000'0000;
		static constexpr std::uint64_t address_stride = 16;

		[[nodiscard]] std::uint64_t frame_address(const rml::luau::mirror::CallInfo& frame);

		std::unique_ptr<Entry[]> m_entries;
		std::atomic<std::uint64_t> m_dropped{};
	};
}
