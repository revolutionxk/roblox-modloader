#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace rml::memory
{
	struct AnchoredFunction
	{
		void* start;
		std::size_t size;
	};

	RML_EXPORT std::vector<AnchoredFunction> functions_referencing_string(std::string_view exact_text);
	RML_EXPORT std::vector<std::vector<AnchoredFunction>> functions_referencing_strings(std::span<const std::string_view> exact_texts);
	RML_EXPORT std::vector<AnchoredFunction> functions_calling(const void* target);
	RML_EXPORT std::vector<void*> calls_from(const AnchoredFunction& function);
	RML_EXPORT std::vector<void*> data_references_from(const AnchoredFunction& function);
	RML_EXPORT std::optional<AnchoredFunction> function_containing(const void* address);
}
