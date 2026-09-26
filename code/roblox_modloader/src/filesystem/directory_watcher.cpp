#include "directory_watcher.hpp"

#include "RobloxModLoader/util/hash.hpp"

#include <algorithm>

namespace rml::filesystem
{
	using namespace std::chrono_literals;

	DirectoryWatcher::~DirectoryWatcher()
	{
		stop();
	}

	void DirectoryWatcher::start(std::vector<Root> roots, Options options, Callback on_changed)
	{
		stop();

		if (roots.empty() || !on_changed)
		{
			return;
		}

		std::vector<Tracked> tracked;
		tracked.reserve(roots.size());

		for (auto& root : roots)
		{
			const auto fingerprint = fingerprint_of(root.path, options.extensions);
			tracked.push_back(Tracked{.root = std::move(root), .fingerprint = fingerprint});
		}

		m_thread = std::jthread([tracked = std::move(tracked), options = std::move(options),
		                         on_changed = std::move(on_changed)](const std::stop_token& stop_token) mutable {
			run(stop_token, std::move(tracked), options, on_changed);
		});
	}

	void DirectoryWatcher::stop()
	{
		if (!m_thread.joinable())
		{
			return;
		}

		m_thread.request_stop();
		m_thread.join();
	}

	std::size_t DirectoryWatcher::fingerprint_of(const std::filesystem::path& root,
	                                             const std::vector<std::string>& extensions)
	{
		std::error_code ec;
		if (!std::filesystem::is_directory(root, ec))
		{
			return 0;
		}

		std::filesystem::recursive_directory_iterator it(
		    root, std::filesystem::directory_options::skip_permission_denied, ec);
		if (ec)
		{
			return 0;
		}

		std::size_t fingerprint = utils::fnv64_offset;

		const std::filesystem::recursive_directory_iterator end;
		for (; it != end; it.increment(ec))
		{
			if (ec)
			{
				break;
			}

			std::error_code entry_error;
			if (!it->is_regular_file(entry_error) || entry_error)
			{
				continue;
			}

			const auto& path = it->path();

			if (!extensions.empty())
			{
				const auto extension = path.extension().string();
				if (std::ranges::find(extensions, extension) == extensions.end())
				{
					continue;
				}
			}

			utils::hash_combine(fingerprint, std::hash<std::string>{}(path.generic_string()));

			const auto written = std::filesystem::last_write_time(path, entry_error);
			if (!entry_error)
			{
				utils::hash_combine(fingerprint, static_cast<std::size_t>(written.time_since_epoch().count()));
			}
		}

		return fingerprint;
	}

	void DirectoryWatcher::run(const std::stop_token& stop_token, std::vector<Tracked> tracked, const Options& options,
	                           const Callback& on_changed)
	{
		constexpr auto step = 100ms;

		while (!stop_token.stop_requested())
		{
			for (auto elapsed = 0ms; elapsed < options.poll_interval && !stop_token.stop_requested(); elapsed += step)
			{
				std::this_thread::sleep_for(step);
			}

			if (stop_token.stop_requested())
			{
				break;
			}

			const auto now = std::chrono::steady_clock::now();

			for (auto& entry : tracked)
			{
				const auto fingerprint = fingerprint_of(entry.root.path, options.extensions);

				if (fingerprint != entry.fingerprint)
				{
					entry.fingerprint = fingerprint;
					entry.dirty = true;
					entry.changed_at = now;
					continue;
				}

				if (!entry.dirty || now - entry.changed_at < options.settle)
				{
					continue;
				}

				entry.dirty = false;
				on_changed(entry.root.label);
			}
		}
	}
}
