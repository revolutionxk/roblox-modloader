#pragma once

#include "theme_definition.hpp"

#include <nlohmann/json.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace studio_theme
{
	[[nodiscard]] std::expected<ThemeDefinition, std::string> parse_theme(const nlohmann::json& json, bool builtin);
	[[nodiscard]] nlohmann::json serialize_theme(const ThemeDefinition& theme);

	class ThemeStore
	{
	public:
		explicit ThemeStore(std::filesystem::path root);

		void load();

		[[nodiscard]] const std::vector<ThemeDefinition>& themes() const
		{
			return m_themes;
		}

		[[nodiscard]] const std::optional<std::string>& active() const
		{
			return m_active;
		}

		[[nodiscard]] const ThemeDefinition* find(std::string_view name) const;
		std::expected<void, std::string> save(ThemeDefinition theme);
		void set_active(std::optional<std::string> name);

	private:
		void read_folder(const std::filesystem::path& folder, bool builtin);
		[[nodiscard]] std::filesystem::path state_file() const;

		std::filesystem::path m_root;
		std::vector<ThemeDefinition> m_themes;
		std::optional<std::string> m_active;
	};
}
