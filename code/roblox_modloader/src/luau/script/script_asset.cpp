#include "RobloxModLoader/luau/script/script_asset.hpp"

#include "RobloxModLoader/util/filesystem.hpp"

namespace rml::luau
{
	bool is_script_file(const std::filesystem::path& path)
	{
		return std::ranges::find(kSourceExtensions, path.extension().string()) != kSourceExtensions.end();
	}

	bool is_init_file(const std::filesystem::path& path)
	{
		return path.stem() == kInitStem;
	}

	std::expected<std::string, std::string> read_source(const std::filesystem::path& path)
	{
		std::error_code error;
		const auto status = std::filesystem::status(path, error);
		if (error)
		{
			return std::unexpected(std::format("cannot stat '{}': {}", path.string(), error.message()));
		}

		if (!std::filesystem::is_regular_file(status))
		{
			return std::unexpected(std::format("not a regular file: '{}'", path.string()));
		}

		auto source = utils::read_file(path);
		if (!source)
		{
			return std::unexpected(std::format("cannot read '{}': {}", path.string(), source.error().message()));
		}

		return std::move(*source);
	}

	std::optional<std::filesystem::file_time_type> file_mtime(const std::filesystem::path& path) noexcept
	{
		std::error_code error;
		const auto time = std::filesystem::last_write_time(path, error);
		if (error)
		{
			return std::nullopt;
		}

		return time;
	}
}
