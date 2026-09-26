#include "target/call_path.hpp"

#include <cstring>
#include <format>

namespace rml::dumper::target
{
	static constexpr std::uint8_t call_opcode = 0xE8;
	static constexpr std::uint8_t jump_opcode = 0xE9;
	static constexpr std::uint8_t padding_opcode = 0xCC;

	static bool is_call_target(const image::Image& image, const Rva address)
	{
		if (image.functions().at(address))
			return true;

		if (image.architecture() != Architecture::x86_64 || address == 0 || (address & 0xF) != 0)
			return false;

		const auto* section = image.section_containing(address);
		if (!section || !section->executable)
			return false;

		const auto before = image.at(address - 1, 1);
		return !before.empty() && static_cast<std::uint8_t>(before.front()) == padding_opcode;
	}

	static std::vector<Rva> x86_calls(const image::Image& image, const index::FunctionBounds& function)
	{
		std::vector<Rva> targets;
		const auto code = image.at(function.begin, function.size());

		for (std::size_t offset = 0; offset + 5 <= code.size(); ++offset)
		{
			const auto opcode = static_cast<std::uint8_t>(code[offset]);
			if (opcode != call_opcode && opcode != jump_opcode)
				continue;

			std::int32_t displacement = 0;
			std::memcpy(&displacement, code.data() + offset + 1, sizeof(displacement));
			const auto target = static_cast<Rva>(static_cast<std::int64_t>(function.begin + offset + 5) + displacement);

			if (opcode == jump_opcode && target >= function.begin && target < function.end)
				continue;
			if (is_call_target(image, target))
				targets.push_back(target);
		}

		return targets;
	}

	static std::vector<Rva> arm64_calls(const image::Image& image, const index::FunctionBounds& function)
	{
		std::vector<Rva> targets;
		const auto code = image.at(function.begin, function.size());

		for (std::size_t offset = 0; offset + 4 <= code.size(); offset += 4)
		{
			std::uint32_t instruction = 0;
			std::memcpy(&instruction, code.data() + offset, sizeof(instruction));
			if ((instruction & 0x7C000000) != 0x14000000)
				continue;

			std::int64_t immediate = instruction & 0x03FFFFFF;
			if (immediate & 0x02000000)
				immediate -= 0x04000000;

			const auto target = static_cast<Rva>(static_cast<std::int64_t>(function.begin + offset) + immediate * 4);
			const bool linked = (instruction & 0x80000000) != 0;

			if (!linked && target >= function.begin && target < function.end)
				continue;
			if (is_call_target(image, target))
				targets.push_back(target);
		}

		return targets;
	}

	std::vector<Rva> calls_from(const image::Image& image, const index::FunctionBounds& function)
	{
		return image.architecture() == Architecture::x86_64 ? x86_calls(image, function) : arm64_calls(image, function);
	}

	std::expected<Rva, std::string> follow_path(const image::Image& image, const Rva start, const std::span<const PathStep> path)
	{
		auto current = start;
		for (const auto& [step, index] : path)
		{
			if (step == Step::none)
				break;

			const auto function = image.functions().at(current);
			if (!function)
				return std::unexpected(std::format("0x{:X} is not the start of a function", current));

			const auto targets = calls_from(image, *function);
			if (index >= targets.size())
				return std::unexpected(std::format("0x{:X} has {} calls, call {} was expected", current, targets.size(), index));

			current = step == Step::call ? targets[index] : targets[targets.size() - 1 - index];
		}

		return current;
	}
}
