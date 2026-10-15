#include "engine.hpp"

#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/memory/anchor.hpp>

RML_LOG_SCOPE("StudioTheme")

namespace studio_theme
{
	template<typename Fn>
	static Fn resolve(const char* const name, const rml::memory::anchor_path& path)
	{
		const auto found = rml::memory::locate(path);
		if (!found)
		{
			RML_WARN("{} not resolved: {}", name, found.error());
			return nullptr;
		}
		RML_INFO("Resolved {} at {}", name, *found);
		return reinterpret_cast<Fn>(*found);
	}

	Engine resolve_engine()
	{
		static constexpr auto parse_theme_json = rml::memory::referencing("Invalid Json data");
		static constexpr auto set_current_theme = rml::memory::referencing("previousTheme");
		static constexpr auto core_studio_settings = rml::memory::referencing("%1: %2f/s %3%").call(0);

		Engine engine;
		engine.parse_theme_json = resolve<decltype(engine.parse_theme_json)>("Theme::parseThemeJson", parse_theme_json);
		engine.set_current_theme = resolve<decltype(engine.set_current_theme)>("CoreStudioSettings::setCurrentTheme", set_current_theme);
		engine.core_studio_settings = resolve<decltype(engine.core_studio_settings)>("CoreStudioSettings::singleton", core_studio_settings);
		if (!engine.complete())
			RML_WARN("The Qt shell recolor is off on this Studio build; plugins still follow the theme");
		return engine;
	}
}
