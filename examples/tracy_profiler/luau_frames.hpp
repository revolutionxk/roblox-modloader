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
		static constexpr std::size_t capacity = std::size_t{1} << 16;
		static constexpr std::size_t probe_limit = 64;
		static constexpr unsigned cache_bits = 10;
		static constexpr std::size_t cache_size = std::size_t{1} << cache_bits;
		static constexpr std::uint64_t address_base = 0x0050'0000'0000'0000;
		static constexpr unsigned line_bits = 20;
		static constexpr std::uint32_t line_mask = (std::uint32_t{1} << line_bits) - 1;
		static constexpr std::uint32_t no_index = ~std::uint32_t{};

		struct Entry
		{
			std::atomic<std::uint64_t> key{};
			std::atomic<bool> ready{};
			bool native{};
			std::array<char, 64> name{};
			std::array<char, 128> script{};
		};

		struct CacheSlot
		{
			const void* proto{};
			const void* source{};
			std::int32_t linedefined{};
			std::uint32_t index{no_index};
		};

		[[nodiscard]] std::uint64_t frame_address(const rml::luau::mirror::CallInfo& frame);
		[[nodiscard]] std::uint32_t intern(const rml::luau::mirror::Closure& closure, const rml::luau::mirror::Proto* proto);
		static void publish(Entry& entry, const rml::luau::mirror::Closure& closure, const rml::luau::mirror::Proto* proto);

		std::unique_ptr<Entry[]> m_entries;
		std::atomic<std::uint64_t> m_dropped{};
	};
}
