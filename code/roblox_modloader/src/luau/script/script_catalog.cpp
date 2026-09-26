#include "RobloxModLoader/luau/script/script_catalog.hpp"

#include "RobloxModLoader/config/config_serialization.hpp"
#include "RobloxModLoader/config/config_types.hpp"
#include "filesystem/file.hpp"
#include "RobloxModLoader/roblox/data_model.hpp"

RML_LOG_SCOPE("ScriptCatalog");

namespace rml::luau
{
	std::span<const ScriptAsset> ModScripts::for_context(const RBX::DataModelType type) const noexcept
	{
		const auto it = by_context.find(type);
		if (it == by_context.end())
		{
			return {};
		}

		return it->second;
	}

	PatternSet::PatternSet(const std::span<const std::string> patterns)
	{
		m_patterns.reserve(patterns.size());

		for (const auto& pattern : patterns)
		{
			try
			{
				m_patterns.emplace_back(translate(pattern), std::regex_constants::icase);
			}
			catch (const std::regex_error& error)
			{
				RML_WARN("Ignoring invalid script pattern '{}': {}", pattern, error.what());
			}
		}
	}

	bool PatternSet::matches(const std::filesystem::path& relative) const
	{
		const auto text = relative.generic_string();

		return std::ranges::any_of(m_patterns, [&text](const std::regex& expression) {
			return std::regex_match(text, expression);
		});
	}

	std::string PatternSet::translate(const std::string_view glob)
	{
		std::string out;
		out.reserve(glob.size() * 2);

		for (std::size_t i = 0; i < glob.size(); ++i)
		{
			switch (const auto c = glob[i])
			{
			case '.':
			case '^':
			case '$':
			case '+':
			case '{':
			case '}':
			case '[':
			case ']':
			case '(':
			case ')':
			case '\\':
			case '|':
				out += '\\';
				out += c;
				break;

			case '*':
				if (i + 1 < glob.size() && glob[i + 1] == '*')
				{
					out += ".*";
					++i;
				}
				else
				{
					out += "[^/]*";
				}
				break;

			case '?':
				out += "[^/]";
				break;

			default:
				out += c;
				break;
			}
		}

		return out;
	}

	std::vector<ScriptAsset> collect_scripts(const std::filesystem::path& scripts_root, const PatternSet& patterns)
	{
		std::vector<ScriptAsset> assets;

		if (patterns.empty())
		{
			return assets;
		}

		std::error_code error;
		if (!std::filesystem::is_directory(scripts_root, error))
		{
			return assets;
		}

		std::filesystem::recursive_directory_iterator it(
		    scripts_root, std::filesystem::directory_options::skip_permission_denied, error);
		if (error)
		{
			RML_WARN("Cannot walk '{}': {}", scripts_root.string(), error.message());
			return assets;
		}

		const std::filesystem::recursive_directory_iterator end;
		for (; it != end; it.increment(error))
		{
			if (error)
			{
				RML_WARN("Stopped walking '{}': {}", scripts_root.string(), error.message());
				break;
			}

			const auto& path = it->path();

			std::error_code entry_error;
			if (!it->is_regular_file(entry_error) || entry_error)
			{
				continue;
			}

			if (!is_script_file(path))
			{
				continue;
			}

			const auto relative = std::filesystem::relative(path, scripts_root, entry_error);
			if (entry_error || relative.empty())
			{
				continue;
			}

			if (!patterns.matches(relative))
			{
				continue;
			}

			ScriptAsset asset{.path = path};
			if (const auto mtime = file_mtime(path))
			{
				asset.mtime = *mtime;
			}

			assets.push_back(std::move(asset));
		}

		return assets;
	}

	std::expected<void, std::string> ScriptCatalog::scan(const std::filesystem::path& mods_directory)
	{
		m_root = mods_directory;
		m_mods.clear();

		std::error_code error;
		if (!std::filesystem::is_directory(mods_directory, error))
		{
			return std::unexpected(std::format("mods directory is not a directory: '{}'", mods_directory.string()));
		}

		std::filesystem::directory_iterator it(mods_directory, error);
		if (error)
		{
			return std::unexpected(
			    std::format("cannot list mods directory '{}': {}", mods_directory.string(), error.message()));
		}

		const std::filesystem::directory_iterator end;
		for (; it != end; it.increment(error))
		{
			if (error)
			{
				RML_WARN("Stopped listing '{}': {}", mods_directory.string(), error.message());
				break;
			}

			const auto& mod_directory = it->path();

			std::error_code entry_error;
			if (!it->is_directory(entry_error) || entry_error)
			{
				continue;
			}

			if (!std::filesystem::exists(mod_directory / "mod.toml", entry_error) || entry_error)
			{
				continue;
			}

			auto mod = load_mod(mod_directory);
			if (!mod)
			{
				RML_ERROR("Skipping mod '{}': {}", mod_directory.filename().string(), mod.error());
				continue;
			}

			m_mods.push_back(std::move(*mod));
		}

		RML_INFO("Discovered {} mods under '{}'", m_mods.size(), mods_directory.string());
		return {};
	}

	std::expected<void, std::string> ScriptCatalog::rescan(const std::string_view mod_name)
	{
		const auto it = std::ranges::find_if(m_mods, [mod_name](const ModScripts& mod) {
			return mod.manifest && mod.manifest->name == mod_name;
		});

		if (it == m_mods.end())
		{
			return std::unexpected(std::format("unknown mod: '{}'", mod_name));
		}

		auto reloaded = load_mod(it->manifest->root);
		if (!reloaded)
		{
			return std::unexpected(std::move(reloaded.error()));
		}

		*it = std::move(*reloaded);

		RML_INFO("Rescanned mod '{}'", mod_name);
		return {};
	}

	const ModScripts* ScriptCatalog::find(const std::string_view mod_name) const noexcept
	{
		for (const auto& mod : m_mods)
		{
			if (mod.manifest && mod.manifest->name == mod_name)
			{
				return &mod;
			}
		}

		return nullptr;
	}

	std::expected<ModScripts, std::string> ScriptCatalog::load_mod(const std::filesystem::path& mod_directory)
	{
		const auto config_path = mod_directory / "mod.toml";

		const auto parsed = filesystem::File(config_path).read_toml();
		if (!parsed)
		{
			return std::unexpected(std::format("cannot read '{}': {}", config_path.string(), parsed.error()));
		}

		const auto mod_config = config::serialization::mod_config_from_toml(*parsed);
		if (!mod_config)
		{
			return std::unexpected(std::format("invalid mod config '{}' (error {})", config_path.string(),
			                                   static_cast<int>(mod_config.error())));
		}

		ModScripts scripts;

		const auto scripts_root = mod_directory / "scripts";
		const auto& contexts = mod_config->datamodel_context;

		const std::array<std::pair<RBX::DataModelType, const std::vector<std::string>*>, 4> sources{{
		    {RBX::DataModelType::Standalone, &contexts.standalone},
		    {RBX::DataModelType::Edit, &contexts.edit},
		    {RBX::DataModelType::Client, &contexts.client},
		    {RBX::DataModelType::Server, &contexts.server},
		}};

		for (const auto& [type, patterns] : sources)
		{
			if (patterns->empty())
			{
				continue;
			}

			const PatternSet set{std::span<const std::string>{*patterns}};
			auto assets = collect_scripts(scripts_root, set);

			RML_DEBUG("Mod '{}' context {}: {} scripts", mod_config->name, std::to_underlying(type), assets.size());

			scripts.by_context.emplace(type, std::move(assets));
		}

		std::vector<std::filesystem::path> entries;
		for (const auto& assets : scripts.by_context | std::views::values)
		{
			for (const auto& asset : assets)
			{
				entries.push_back(asset.path);
			}
		}

		scripts.manifest = std::make_shared<const ModManifest>(ModManifest{
		    .name = mod_config->name,
		    .version = mod_config->version,
		    .author = mod_config->author,
		    .description = mod_config->description,
		    .root = mod_directory,
		    .scripts = build_script_tree(scripts_root, mod_config->name, "self", entries),
		});

		return scripts;
	}
}
