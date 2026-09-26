#pragma once

#include <cstdint>
#include <optional>

namespace rml::memory::instruction
{
	[[nodiscard]] constexpr std::uintptr_t x64_relative_target(const std::uintptr_t next_instruction, const std::int32_t displacement) noexcept
	{
		return next_instruction + static_cast<std::intptr_t>(displacement);
	}

	[[nodiscard]] constexpr bool arm64_is_adrp(const std::uint32_t instruction) noexcept
	{
		return (instruction & 0x9F000000) == 0x90000000;
	}

	[[nodiscard]] constexpr bool arm64_is_branch(const std::uint32_t instruction) noexcept
	{
		return (instruction & 0x7C000000) == 0x14000000;
	}

	[[nodiscard]] constexpr bool arm64_is_linked_branch(const std::uint32_t instruction) noexcept
	{
		return (instruction & 0x80000000) != 0;
	}

	[[nodiscard]] constexpr std::uint32_t arm64_rd(const std::uint32_t instruction) noexcept
	{
		return instruction & 0x1F;
	}

	[[nodiscard]] constexpr std::uint32_t arm64_rn(const std::uint32_t instruction) noexcept
	{
		return (instruction >> 5) & 0x1F;
	}

	[[nodiscard]] constexpr std::uintptr_t arm64_adrp_page(const std::uintptr_t pc, const std::uint32_t adrp) noexcept
	{
		const std::int64_t low = (adrp >> 29) & 0x3;
		const std::int64_t high = (adrp >> 5) & 0x7FFFF;
		std::int64_t offset = ((high << 2) | low) << 12;
		if (offset & (std::int64_t{1} << 32))
			offset -= std::int64_t{1} << 33;
		return static_cast<std::uintptr_t>(static_cast<std::int64_t>(pc & ~std::uintptr_t{0xFFF}) + offset);
	}

	[[nodiscard]] constexpr std::optional<std::uintptr_t> arm64_page_offset(const std::uint32_t next) noexcept
	{
		const std::uintptr_t immediate = (next >> 10) & 0xFFF;
		if ((next & 0xFF800000) == 0x91000000)
			return ((next >> 22) & 0x3) == 1 ? immediate << 12 : immediate;
		if ((next & 0x3B000000) == 0x39000000)
			return immediate << (next >> 30);
		return std::nullopt;
	}

	[[nodiscard]] constexpr std::uintptr_t arm64_branch_target(const std::uintptr_t pc, const std::uint32_t branch) noexcept
	{
		std::int64_t offset = branch & 0x03FFFFFF;
		if (offset & 0x02000000)
			offset -= 0x04000000;
		return static_cast<std::uintptr_t>(static_cast<std::int64_t>(pc) + offset * 4);
	}
}
