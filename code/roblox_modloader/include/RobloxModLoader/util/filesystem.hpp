#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>
#include <string>
#include <string_view>
#include <system_error>

namespace rml::utils
{
	[[nodiscard]] inline std::filesystem::path canonical_or_self(const std::filesystem::path& path)
	{
		std::error_code ec;
		auto canonical = std::filesystem::weakly_canonical(path, ec);
		return ec ? path : canonical;
	}

	[[nodiscard]] inline std::string with_trailing_slash(const std::filesystem::path& path)
	{
		auto text = path.generic_string();
		if (!text.empty() && !text.ends_with('/'))
			text += '/';
		return text;
	}

	[[nodiscard]] inline bool is_under(const std::filesystem::path& root, const std::filesystem::path& candidate)
	{
		std::error_code ec;
		const auto relative = std::filesystem::relative(candidate, root, ec);
		if (ec || relative.empty())
			return false;
		return *relative.begin() != "..";
	}

	[[nodiscard]] inline std::expected<std::string, std::error_code> read_file(const std::filesystem::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
			return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));

		std::string contents{std::istreambuf_iterator<char>(stream), {}};
		if (stream.bad())
			return std::unexpected(std::make_error_code(std::errc::io_error));
		return contents;
	}

	[[nodiscard]] inline std::expected<void, std::error_code> write_file(const std::filesystem::path& path, const std::string_view bytes)
	{
		std::error_code ec;
		if (path.has_parent_path())
			std::filesystem::create_directories(path.parent_path(), ec);
		if (ec)
			return std::unexpected(ec);

		std::ofstream stream(path, std::ios::binary | std::ios::trunc);
		if (!stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size())))
			return std::unexpected(std::make_error_code(std::errc::io_error));
		return {};
	}

	[[nodiscard]] inline std::expected<void, std::error_code> write_file_atomic(const std::filesystem::path& path, const std::string_view bytes)
	{
		auto staging = path;
		staging += std::format(".{:08x}.tmp", std::random_device{}());

		if (auto written = write_file(staging, bytes); !written)
			return written;

		std::error_code ec;
		std::filesystem::rename(staging, path, ec);
		if (ec)
		{
			std::filesystem::remove(staging, ec);
			return std::unexpected(ec);
		}
		return {};
	}

	template<class Map, class Projection = std::identity>
	std::size_t erase_under(Map& map, const std::filesystem::path& root, Projection project = {})
	{
		const auto prefix = with_trailing_slash(canonical_or_self(root));
		return std::erase_if(map, [&](const auto& entry) {
			return std::string_view(std::invoke(project, entry.first)).starts_with(prefix);
		});
	}
}
