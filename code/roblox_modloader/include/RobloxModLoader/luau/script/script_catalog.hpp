#pragma once

#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "RobloxModLoader/luau/script/mod_manifest.hpp"
#include "RobloxModLoader/luau/script/script_asset.hpp"
#include <unordered_map>

namespace rml::config
{
	struct ModConfig;
}

namespace rml::luau
{
	struct ModScripts
	{
		ModManifestPtr manifest;
		std::unordered_map<RBX::DataModelType, std::vector<ScriptAsset>> by_context;

		[[nodiscard]] std::span<const ScriptAsset> for_context(RBX::DataModelType type) const noexcept;
	};

	class RML_EXPORT ScriptCatalog final
	{
	public:
		std::expected<void, std::string> scan(const std::filesystem::path& mods_directory);

		std::expected<void, std::string> rescan(std::string_view mod_name);

		[[nodiscard]] std::span<const ModScripts> mods() const noexcept { return m_mods; }

		[[nodiscard]] const ModScripts* find(std::string_view mod_name) const noexcept;

		void clear() noexcept { m_mods.clear(); }

	private:
		[[nodiscard]] static std::expected<ModScripts, std::string> load_mod(
		    const std::filesystem::path& mod_directory);

		std::filesystem::path m_root;
		std::vector<ModScripts> m_mods;
	};

	class PatternSet final
	{
	public:
		explicit PatternSet(std::span<const std::string> patterns);

		[[nodiscard]] bool matches(const std::filesystem::path& relative) const;

		[[nodiscard]] bool empty() const noexcept { return m_patterns.empty(); }

		[[nodiscard]] static std::string translate(std::string_view glob);

	private:
		std::vector<std::regex> m_patterns;
	};

	[[nodiscard]] std::vector<ScriptAsset> collect_scripts(
	    const std::filesystem::path& scripts_root, const PatternSet& patterns);
}
