#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace rml::platform
{
	[[nodiscard]] RML_EXPORT std::size_t capture_return_addresses(std::span<std::uintptr_t> frames) noexcept;
	[[nodiscard]] RML_EXPORT bool frame_established(const void* pc) noexcept;
}
