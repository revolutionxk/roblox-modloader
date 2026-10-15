#include "theme_store.hpp"

#include <RobloxModLoader/logger/logger.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

RML_LOG_SCOPE("StudioTheme")

namespace studio_theme
{
	static std::optional<std::string> read_file(const std::filesystem::path& file)
	{
		std::ifstream stream{file, std::ios::binary};
		if (!stream)
			return std::nullopt;
		return std::string{std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
	}

	static std::expected<void, std::string> write_file(const std::filesystem::path& file, const std::string& text)
	{
		std::ofstream stream{file, std::ios::binary | std::ios::trunc};
		if (!stream)
			return std::unexpected(std::format("cannot write {}", file.string()));
		stream << text;
		return {};
	}

	static std::string slug(const std::string_view name)
	{
		std::string out;
		for (const unsigned char c : name)
		{
			if (std::isalnum(c))
				out.push_back(static_cast<char>(std::tolower(c)));
			else if (!out.empty() && out.back() != '-')
				out.push_back('-');
		}
		while (!out.empty() && out.back() == '-')
			out.pop_back();
		return out.empty() ? std::string{"theme"} : out;
	}

	static std::expected<Color, std::string> color_value(const nlohmann::json& value, const std::string& where)
	{
		if (!value.is_string())
			return std::unexpected(std::format("{} must be a #RRGGBB string", where));
		const auto parsed = parse_hex(value.get_ref<const std::string&>());
		if (!parsed)
			return std::unexpected(std::format("{} is not a #RRGGBB color", where));
		return *parsed;
	}

	std::expected<ThemeDefinition, std::string> parse_theme(const nlohmann::json& json, const bool builtin)
	{
		if (!json.is_object())
			return std::unexpected("a theme must be a JSON object");

		ThemeDefinition theme;
		theme.builtin = builtin;

		const auto name = json.find("name");
		if (name == json.end() || !name->is_string() || name->get_ref<const std::string&>().empty())
			return std::unexpected("\"name\" must be a non-empty string");
		theme.name = name->get<std::string>();

		if (const auto base = json.find("base"); base != json.end())
		{
			if (*base == "Light")
				theme.base = Base::Light;
			else if (*base != "Dark")
				return std::unexpected("\"base\" must be \"Dark\" or \"Light\"");
		}

		const auto colors = json.find("colors");
		if (colors == json.end() || !colors->is_object())
			return std::unexpected("\"colors\" must be an object");
		for (std::size_t i = 0; i < k_role_names.size(); ++i)
		{
			const std::string key{k_role_names[i]};
			const auto value = colors->find(key);
			if (value == colors->end())
				return std::unexpected(std::format("colors.{} is missing", key));
			const auto parsed = color_value(*value, "colors." + key);
			if (!parsed)
				return std::unexpected(parsed.error());
			theme.roles[i] = *parsed;
		}

		if (const auto syntax = json.find("syntax"); syntax != json.end() && !syntax->is_null())
		{
			if (!syntax->is_object())
				return std::unexpected("\"syntax\" must be an object");
			for (const auto& [key, value] : syntax->items())
			{
				const auto parsed = color_value(value, "syntax." + key);
				if (!parsed)
					return std::unexpected(parsed.error());
				theme.syntax.emplace(key, *parsed);
			}
		}

		if (const auto overrides = json.find("overrides"); overrides != json.end() && !overrides->is_null())
		{
			if (!overrides->is_object())
				return std::unexpected("\"overrides\" must be an object");
			for (const auto& [key, value] : overrides->items())
			{
				const auto stock = parse_hex(key);
				if (!stock)
					return std::unexpected(std::format("override key {} is not a #RRGGBB color", key));
				const auto parsed = color_value(value, "overrides." + key);
				if (!parsed)
					return std::unexpected(parsed.error());
				theme.overrides.emplace(rgb_key(*stock), *parsed);
			}
		}

		return theme;
	}

	nlohmann::json serialize_theme(const ThemeDefinition& theme)
	{
		nlohmann::json json;
		json["name"] = theme.name;
		json["base"] = std::string{base_name(theme.base)};

		auto& colors = json["colors"] = nlohmann::json::object();
		for (std::size_t i = 0; i < k_role_names.size(); ++i)
			colors[std::string{k_role_names[i]}] = to_hex(theme.roles[i], false);

		auto& syntax = json["syntax"] = nlohmann::json::object();
		for (const auto& [key, value] : theme.syntax)
			syntax[key] = to_hex(value, false);

		auto& overrides = json["overrides"] = nlohmann::json::object();
		for (const auto& [key, value] : theme.overrides)
			overrides[std::format("#{:06X}", key)] = to_hex(value, false);

		return json;
	}

	ThemeStore::ThemeStore(std::filesystem::path root) :
	    m_root(std::move(root))
	{
	}

	void ThemeStore::load()
	{
		m_themes.clear();
		read_folder(m_root / "themes" / "presets", true);
		read_folder(m_root / "themes" / "user", false);

		m_active.reset();
		const auto text = read_file(state_file());
		if (!text)
			return;

		const auto state = nlohmann::json::parse(*text, nullptr, false);
		if (state.is_discarded() || !state.is_object())
		{
			RML_WARN("Ignoring {}: not a JSON object", state_file().string());
			return;
		}
		if (const auto active = state.find("active"); active != state.end() && active->is_string())
			m_active = active->get<std::string>();
	}

	void ThemeStore::read_folder(const std::filesystem::path& folder, const bool builtin)
	{
		std::error_code error;
		std::vector<std::filesystem::path> files;
		for (const auto& entry : std::filesystem::directory_iterator{folder, error})
			if (entry.is_regular_file(error) && entry.path().extension() == ".json")
				files.push_back(entry.path());
		std::ranges::sort(files);

		for (const auto& file : files)
		{
			const auto text = read_file(file);
			if (!text)
			{
				RML_WARN("Skipping theme {}: cannot read it", file.string());
				continue;
			}

			const auto json = nlohmann::json::parse(*text, nullptr, false);
			if (json.is_discarded())
			{
				RML_WARN("Skipping theme {}: not valid JSON", file.string());
				continue;
			}

			auto theme = parse_theme(json, builtin);
			if (!theme)
			{
				RML_WARN("Skipping theme {}: {}", file.string(), theme.error());
				continue;
			}

			if (find(theme->name))
			{
				RML_WARN("Skipping theme {}: a theme named '{}' is already loaded", file.string(), theme->name);
				continue;
			}

			m_themes.push_back(std::move(*theme));
		}
	}

	const ThemeDefinition* ThemeStore::find(const std::string_view name) const
	{
		const auto it = std::ranges::find(m_themes, name, &ThemeDefinition::name);
		return it == m_themes.end() ? nullptr : &*it;
	}

	std::expected<void, std::string> ThemeStore::save(ThemeDefinition theme)
	{
		if (const auto* existing = find(theme.name); existing && existing->builtin)
			return std::unexpected(std::format("'{}' is a built-in theme, save it under another name", theme.name));

		const auto folder = m_root / "themes" / "user";
		std::error_code error;
		std::filesystem::create_directories(folder, error);
		if (error)
			return std::unexpected(std::format("cannot create {}: {}", folder.string(), error.message()));

		if (auto written = write_file(folder / (slug(theme.name) + ".json"), serialize_theme(theme).dump(1, '\t') + "\n"); !written)
			return written;

		theme.builtin = false;
		if (const auto it = std::ranges::find(m_themes, theme.name, &ThemeDefinition::name); it != m_themes.end())
			*it = std::move(theme);
		else
			m_themes.push_back(std::move(theme));
		return {};
	}

	void ThemeStore::set_active(std::optional<std::string> name)
	{
		m_active = std::move(name);

		nlohmann::json state;
		state["active"] = m_active ? nlohmann::json(*m_active) : nlohmann::json(nullptr);
		if (const auto written = write_file(state_file(), state.dump(1, '\t') + "\n"); !written)
			RML_WARN("The theme selection was not saved: {}", written.error());
	}

	std::filesystem::path ThemeStore::state_file() const
	{
		return m_root / "state.json";
	}
}
