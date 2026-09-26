#pragma once

#include "RobloxModLoader/internal/common.hpp"

#include <array>
#include <string_view>

namespace rml::luau
{
	struct ScriptAsset
	{
		std::filesystem::path path;
		std::filesystem::file_time_type mtime{};

		[[nodiscard]] std::string chunk_name() const { return path.filename().string(); }
	};

	inline constexpr std::array<std::string_view, 2> kSourceExtensions{".luau", ".lua"};
	inline constexpr std::array<std::string_view, 2> kInitNames{"init.luau", "init.lua"};
	inline constexpr std::string_view kInitStem = "init";

	[[nodiscard]] bool is_script_file(const std::filesystem::path& path);
	[[nodiscard]] bool is_init_file(const std::filesystem::path& path);

	[[nodiscard]] std::expected<std::string, std::string> read_source(const std::filesystem::path& path);

	[[nodiscard]] std::optional<std::filesystem::file_time_type> file_mtime(const std::filesystem::path& path) noexcept;
}
