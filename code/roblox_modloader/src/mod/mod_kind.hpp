#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string_view>

namespace rml
{
	enum class ModKind
	{
		Native,
		Dotnet,
		Scripts
	};

	struct ModKindFolder
	{
		ModKind kind;
		std::string_view folder_name;
	};

	inline constexpr std::array<ModKindFolder, 3> kModKindFolders{{
	    {ModKind::Native, "native"},
	    {ModKind::Dotnet, "dotnet"},
	    {ModKind::Scripts, "scripts"},
	}};

	[[nodiscard]] std::optional<ModKind> mod_kind_from_folder(std::string_view folder_name) noexcept;
	[[nodiscard]] std::string_view mod_kind_folder_name(ModKind kind) noexcept;
	[[nodiscard]] bool is_mod_enabled(const std::filesystem::path& mod_root);
}
