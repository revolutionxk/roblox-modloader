#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <filesystem>
#include <string_view>

namespace rml::utils::shell
{
	RML_EXPORT void open(const std::filesystem::path& path);
	RML_EXPORT void open_folder(const std::filesystem::path& path);
	RML_EXPORT void message_box(std::string_view title, std::string_view text);
}
