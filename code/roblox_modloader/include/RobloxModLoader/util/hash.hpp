#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rml::utils
{
	inline constexpr std::uint32_t fnv32_offset = 2166136261u;
	inline constexpr std::uint32_t fnv32_prime = 16777619u;
	inline constexpr std::uint64_t fnv64_offset = 14695981039346656037ull;
	inline constexpr std::uint64_t fnv64_prime = 1099511628211ull;

	[[nodiscard]] constexpr std::uint32_t fnv1a_32(const std::string_view text, std::uint32_t hash = fnv32_offset) noexcept
	{
		for (const char c : text)
			hash = (hash ^ static_cast<unsigned char>(c)) * fnv32_prime;
		return hash;
	}

	[[nodiscard]] constexpr std::uint64_t fnv1a_64(const std::string_view text, std::uint64_t hash = fnv64_offset) noexcept
	{
		for (const char c : text)
			hash = (hash ^ static_cast<unsigned char>(c)) * fnv64_prime;
		return hash;
	}

	constexpr void hash_combine(std::size_t& seed, const std::size_t value) noexcept
	{
		seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
	}
}
