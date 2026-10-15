#include "engine.hpp"
#include "theme_store.hpp"

#include <RobloxModLoader/logger/logger.hpp>
#include <RobloxModLoader/mod/mod_base.hpp>

RML_LOG_SCOPE("StudioTheme")

class StudioTheme final : public ModBase
{
public:
	StudioTheme()
	{
		name = "Studio Theme";
		version = "1.0.0";
		author = "Revolution";
		description = "Recolors all of Roblox Studio from a theme, with a live editor and color picker.";
	}

	void on_load() override
	{
		const auto engine = studio_theme::resolve_engine();
		studio_theme::ThemeStore store{mod_folder()};
		store.load();
		RML_INFO("{} themes loaded, engine {}", store.themes().size(), engine.complete() ? "complete" : "incomplete");
	}

	void on_unload() override
	{
	}
};

extern "C"
{
	RML_MOD_ABI_EXPORT ModBase* start_mod()
	{
		return new StudioTheme();
	}

	RML_MOD_ABI_EXPORT void uninstall_mod(const ModBase* mod)
	{
		delete mod;
	}
}

RML_EXPORT_MOD_ABI_VERSION()
