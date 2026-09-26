#pragma once

#include "RobloxModLoader/memory/string_anchor.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace rml::memory::detail
{
	struct AddressRange
	{
		std::uintptr_t begin{};
		std::uintptr_t end{};
	};

	struct CodeReference
	{
		std::uintptr_t site{};
		std::uintptr_t target{};
	};

	[[nodiscard]] bool image_ready();
	[[nodiscard]] AddressRange image_range();
	[[nodiscard]] std::span<const AddressRange> string_sections();
	[[nodiscard]] std::vector<CodeReference> code_references(std::span<const std::uintptr_t> sorted_targets);
	[[nodiscard]] std::optional<AnchoredFunction> containing_function(std::uintptr_t address);

	[[nodiscard]] std::vector<AnchoredFunction> unique_functions(std::span<const std::uintptr_t> addresses);
}
