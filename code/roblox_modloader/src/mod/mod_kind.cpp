#include "mod_kind.hpp"

#include "RobloxModLoader/config/config_serialization.hpp"
#include "filesystem/file.hpp"

namespace rml
{
	std::optional<ModKind> mod_kind_from_folder(const std::string_view folder_name) noexcept
	{
		for (const auto& entry : kModKindFolders)
		{
			if (entry.folder_name == folder_name)
				return entry.kind;
		}
		return std::nullopt;
	}

	std::string_view mod_kind_folder_name(const ModKind kind) noexcept
	{
		for (const auto& entry : kModKindFolders)
		{
			if (entry.kind == kind)
				return entry.folder_name;
		}
		return {};
	}

	bool is_mod_enabled(const std::filesystem::path& mod_root)
	{
		const auto manifest = mod_root / "mod.toml";
		if (!std::filesystem::exists(manifest))
			return true;

		const auto parsed = filesystem::File(manifest).read_toml();
		if (!parsed)
			return true;

		const auto config = config::serialization::mod_config_from_toml(*parsed);
		return !config || config->runtime.enabled;
	}
}
