#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <cstdint>

namespace rml::platform
{
	RML_EXPORT bool set_output_readable(bool readable);
	[[nodiscard]] RML_EXPORT bool output_readable(std::uint32_t width, std::uint32_t height);
}
