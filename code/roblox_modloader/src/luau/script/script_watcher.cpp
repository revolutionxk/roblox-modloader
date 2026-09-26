#include "script_watcher.hpp"

#include "RobloxModLoader/luau/script/script_asset.hpp"
#include "RobloxModLoader/luau/script_runtime.hpp"

RML_LOG_SCOPE("ScriptWatcher");

namespace rml::luau
{
	ScriptWatcher::ScriptWatcher(ScriptRuntime& runtime) noexcept
		: m_runtime(&runtime)
	{
	}

	ScriptWatcher::~ScriptWatcher()
	{
		stop();
	}

	void ScriptWatcher::start()
	{
		auto roots = m_runtime->script_roots();
		if (roots.empty())
		{
			RML_INFO("No script mods to watch");
			return;
		}

		std::vector<filesystem::DirectoryWatcher::Root> watched;
		watched.reserve(roots.size());

		for (auto& [name, path] : roots)
		{
			watched.push_back(filesystem::DirectoryWatcher::Root{.label = std::move(name), .path = std::move(path)});
		}

		filesystem::DirectoryWatcher::Options options{
		    .poll_interval = std::chrono::milliseconds{500},
		    .settle = std::chrono::milliseconds{250},
		    .extensions = {kSourceExtensions.begin(), kSourceExtensions.end()},
		};

		const auto watched_count = watched.size();

		m_watcher.start(std::move(watched), std::move(options), [this](const std::string& mod_name) {
			if (auto reloaded = m_runtime->reload(mod_name); !reloaded)
			{
				RML_ERROR("Hot reload of '{}' failed: {}", mod_name, reloaded.error());
			}
		});

		RML_INFO("Watching {} script mod(s) for changes", watched_count);
	}

	void ScriptWatcher::stop()
	{
		if (!m_watcher.running())
		{
			return;
		}

		m_watcher.stop();
		RML_INFO("Stopped watching script mods");
	}
}
