#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace rml::platform
{
	struct ImportSlot
	{
		void** slot{};
		void* bound{};
	};

	[[nodiscard]] RML_EXPORT std::vector<ImportSlot> find_import_slots(std::string_view image, std::string_view symbol);
	RML_EXPORT bool rebind_import_slots(std::span<const ImportSlot> slots, void* replacement) noexcept;
	RML_EXPORT void restore_import_slots(std::span<const ImportSlot> slots) noexcept;
}
