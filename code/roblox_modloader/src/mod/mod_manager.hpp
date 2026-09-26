#pragma once

#include "RobloxModLoader/roblox/data_model_type.hpp"
#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/mod/events.hpp"
#include "RobloxModLoader/mod/mod_base.hpp"
#include "imod_loader.hpp"
#include "mod_kind.hpp"

namespace RBX
{
	class DataModel;
}

namespace rml
{
	struct ModManagerError
	{
		enum class Type
		{
			DirectoryNotFound,
			PathNotDirectory,
			NoLoaderFound,
			LoadFailed,
			UnloadFailed,
			ReloadFailed
		};

		Type type;
		std::string message;

		ModManagerError(const Type t, std::string msg) :
		    type(t),
		    message(std::move(msg))
		{
		}
	};

	class ModManager
	{
	public:
		ModManager() = default;
		~ModManager();

		ModManager(const ModManager&) = delete;
		ModManager& operator=(const ModManager&) = delete;

		[[nodiscard]] std::expected<void, ModManagerError> initialize(events::EventManager& event_manager);
		void shutdown();

		void register_loader(std::unique_ptr<IModLoader> loader, ModKind kind);

		[[nodiscard]] std::expected<void, ModManagerError> load_directory(const std::filesystem::path& directory) const;
		[[nodiscard]] std::expected<void, std::string> load(const std::filesystem::path& path) const;
		[[nodiscard]] std::expected<void, std::string> unload(const std::filesystem::path& path) const;
		[[nodiscard]] std::expected<void, std::string> reload(const std::filesystem::path& path) const;

		[[nodiscard]] static std::expected<std::filesystem::path, std::string> get_mods_dir() noexcept;

	private:
		[[nodiscard]] std::optional<ModKind> kind_for_path(const std::filesystem::path& path) const noexcept;
		[[nodiscard]] std::optional<IModLoader*> find_loader_for_path(const std::filesystem::path& path) const noexcept;

		std::unordered_map<ModKind, std::unique_ptr<IModLoader>> m_loaders;
	};
}
