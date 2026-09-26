#include "RobloxModLoader/memory/anchor.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/string_anchor.hpp"

#include <expected>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace rml::memory
{
	std::expected<void*, std::string> follow(const anchor_path& path, void* origin)
	{
		auto* current = origin;
		for (std::uint8_t i = 0; i < path.m_step_count; ++i)
		{
			const auto address = reinterpret_cast<std::uintptr_t>(current);
			if (path.m_steps[i] == anchor_step::caller)
			{
				const auto callers = functions_calling(current);
				if (callers.size() != 1)
					return std::unexpected(std::format("0x{:X} has {} callers", address, callers.size()));
				current = callers.front().start;
				continue;
			}

			const auto function = function_containing(current);
			if (!function || function->start != current)
				return std::unexpected(std::format("0x{:X} is not the start of a function", address));

			const bool data = path.m_steps[i] == anchor_step::data_reference;
			const auto targets = data ? data_references_from(*function) : calls_from(*function);
			const std::size_t index = path.m_indices[i];
			if (index >= targets.size())
				return std::unexpected(std::format("0x{:X} has {} {}, index {} was expected", address, targets.size(), data ? "data references" : "calls", index));

			current = path.m_steps[i] == anchor_step::call_from_end ? targets[targets.size() - 1 - index] : targets[index];
		}
		return current;
	}

	std::expected<void*, std::string> locate(const anchor_path& path)
	{
		if (!*path.m_text.c_str())
			return std::unexpected(std::format("a path from '{}' needs that signature resolved first", path.m_origin.c_str()));

		const std::string_view text = path.m_text.c_str();
		const auto functions = functions_referencing_strings(std::span(&text, 1)).front();
		if (functions.size() != 1)
			return std::unexpected(std::format("{} functions reference \"{}\"", functions.size(), text));
		return follow(path, functions.front().start);
	}
}
