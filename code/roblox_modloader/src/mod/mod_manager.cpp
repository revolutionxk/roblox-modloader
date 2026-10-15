#include "mod_manager.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "dotnet/dotnet_mod_loader.hpp"
#include "native/native_mod_loader.hpp"
#include "filesystem/directory.hpp"

#include <algorithm>
#include <cctype>

RML_LOG_SCOPE("ModManager");

namespace rml
{
	std::expected<void, ModManagerError> ModManager::initialize(events::EventManager& event_manager)
	{
		const auto mods_path = get_mods_dir();

		if (!mods_path.has_value())
		{
			return std::unexpected(ModManagerError(ModManagerError::Type::DirectoryNotFound, mods_path.error()));
		}

		const auto runtime_path = filesystem::directory::get_runtime_directory();

		register_loader(std::make_unique<native::NativeModLoader>(event_manager), ModKind::Native);
		register_loader(std::make_unique<dotnet::DotnetModLoader>(runtime_path,
		                     mods_path.value() / mod_kind_folder_name(ModKind::Dotnet)),
		    ModKind::Dotnet);

		for (auto mod_dir : std::filesystem::directory_iterator(mods_path.value()))
		{
			if (!mod_dir.is_directory())
			{
				continue;
			}

			if (!is_mod_enabled(mod_dir.path()))
			{
				RML_INFO("Skipping '{}': disabled in its mod.toml", mod_dir.path().filename().string());
				continue;
			}

			for (const auto& folder : kModKindFolders)
			{
				auto result = load_directory(mod_dir.path() / folder.folder_name);
				if (!result.has_value())
				{
					switch (result.error().type)
					{
					case ModManagerError::Type::DirectoryNotFound: break;
					case ModManagerError::Type::NoLoaderFound: break;
					default: RML_ERROR("Failed to load {} mods: {}", folder.folder_name, result.error().message); break;
					}
				}
			}
		}

		return {};
	}

	void ModManager::shutdown()
	{
		for (const auto& loader : m_loaders | std::views::values)
			loader->unload_all();
	}

	ModManager::~ModManager()
	{
		shutdown();
	}

	void ModManager::register_loader(std::unique_ptr<IModLoader> loader, const ModKind kind)
	{
		m_loaders[kind] = std::move(loader);
	}

	std::expected<void, ModManagerError> ModManager::load_directory(const std::filesystem::path& directory) const
	{
		if (!std::filesystem::exists(directory))
		{
			return std::unexpected(
			    ModManagerError(ModManagerError::Type::DirectoryNotFound, "Directory does not exist: " + directory.string()));
		}

		if (!std::filesystem::is_directory(directory))
		{
			return std::unexpected(
			    ModManagerError(ModManagerError::Type::PathNotDirectory, "Path is not a directory: " + directory.string()));
		}

		const auto loader = find_loader_for_path(directory);

		if (!loader.has_value())
		{
			return std::unexpected(
			    ModManagerError(ModManagerError::Type::NoLoaderFound, "No loader found for directory: " + directory.string()));
		}

		std::vector<std::string> errors;
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}

			const auto& extensions = (*loader)->extensions();
			if (std::ranges::find_if(extensions,
			        [&entry](const auto& ext) {
				        return entry.path().extension() == ext;
			        })
			    == extensions.end())
			{
				continue;
			}

			if (const auto result = (*loader)->load(entry.path()); !result.has_value())
			{
				errors.push_back("Failed to load " + entry.path().string() + ": " + result.error());
			}
		}

		if (!errors.empty())
		{
			std::string error_message = "Errors occurred while loading mods from directory:\n";
			for (const auto& error : errors)
			{
				error_message += " - " + error + "\n";
			}
			return std::unexpected(ModManagerError(ModManagerError::Type::LoadFailed, std::move(error_message)));
		}

		return {};
	}

	std::expected<void, std::string> ModManager::load(const std::filesystem::path& path) const
	{
		const auto loader = find_loader_for_path(path);
		if (!loader.has_value())
		{
			return std::unexpected("No loader found for file: " + path.string());
		}

		return (*loader)->load(path);
	}

	std::expected<void, std::string> ModManager::unload(const std::filesystem::path& path) const
	{
		const auto loader = find_loader_for_path(path);
		if (!loader.has_value())
		{
			return std::unexpected("No loader found for file: " + path.string());
		}

		return (*loader)->unload(path);
	}

	std::expected<void, std::string> ModManager::reload(const std::filesystem::path& path) const
	{
		const auto loader = find_loader_for_path(path);
		if (!loader.has_value())
		{
			return std::unexpected("No loader found for file: " + path.string());
		}

		return (*loader)->reload(path);
	}

	std::expected<std::filesystem::path, std::string> ModManager::get_mods_dir() noexcept
	{
		auto mod_loader_dir = filesystem::directory::get_mod_loader_directory() / "mods";
		if (!std::filesystem::exists(mod_loader_dir))
		{
			if (std::error_code ec; !std::filesystem::create_directories(mod_loader_dir, ec))
			{
				return std::unexpected("Failed to create mods directory: " + ec.message());
			}
		}

		return std::filesystem::path(mod_loader_dir);
	}

	std::optional<ModKind> ModManager::kind_for_path(const std::filesystem::path& path) const noexcept
	{
		std::error_code ec;
		const bool path_is_directory = std::filesystem::is_directory(path, ec);
		const auto& containing_folder = path_is_directory ? path.filename() : path.parent_path().filename();
		return mod_kind_from_folder(containing_folder.string());
	}

	std::optional<IModLoader*> ModManager::find_loader_for_path(const std::filesystem::path& path) const noexcept
	{
		const auto kind = kind_for_path(path);
		if (!kind.has_value())
		{
			return std::nullopt;
		}

		const auto it = m_loaders.find(*kind);
		if (it == m_loaders.end())
		{
			return std::nullopt;
		}

		return it->second.get();
	}
}
