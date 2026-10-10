#pragma once

#include "rml/dumper/core/types.hpp"
#include "rml/dumper/disasm/register.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace rml::dumper::disasm
{
	using Object = std::uint32_t;

	inline constexpr Object no_object = 0;
	inline constexpr Object zero_object = 255;
	inline constexpr Object first_derived_object = 256;

	[[nodiscard]] constexpr Object entry_object(const Register value)
	{
		return value == Register::none ? no_object : static_cast<Object>(value);
	}

	struct MemoryAccess
	{
		std::size_t sequence{};
		Rva address{};
		Object object{no_object};
		Register base{Register::none};
		Register index{Register::none};
		Register value_register{Register::none};
		Object value_object{no_object};
		std::uint8_t scale{1};
		std::uint8_t width{};
		std::int64_t displacement{};
		std::optional<std::uint64_t> immediate;
		bool is_write{};
		Object loaded{no_object};
	};

	inline constexpr std::size_t register_slots = static_cast<std::size_t>(Register::zr) + 1;

	struct CallSite
	{
		std::size_t sequence{};
		Rva address{};
		std::optional<Rva> target;
		Object callee{no_object};
		std::array<Object, register_slots> registers{};

		[[nodiscard]] Object object_of(const Register value) const
		{
			return registers[static_cast<std::size_t>(value)];
		}
	};

	enum class ConstantKind : std::uint8_t
	{
		scale,
		step,
		literal,
	};

	struct ConstantUse
	{
		std::size_t sequence{};
		Rva address{};
		Register destination{Register::none};
		std::int64_t value{};
		ConstantKind kind{};
		Object operand{no_object};
		Object result{no_object};
	};

	struct Trace
	{
		Rva begin{};
		Rva end{};
		std::vector<MemoryAccess> accesses;
		std::vector<CallSite> calls;
		std::vector<ConstantUse> constants;
	};
}
