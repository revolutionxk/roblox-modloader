#include "controller.hpp"
#include "engine.hpp"

#include <RobloxModLoader/mod/mod_base.hpp>

#include <memory>

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
		m_controller = std::make_unique<studio_theme::Controller>(mod_folder(), studio_theme::resolve_engine());
		m_controller->start();
		register_event_handler<rml::events::DataModelChangedEvent>([this](rml::events::DataModelChangedEvent&) {
			if (m_controller)
				m_controller->on_data_model();
		});
	}

	void on_unload() override
	{
		if (m_controller)
			m_controller->stop();
		m_controller.reset();
	}

private:
	std::unique_ptr<studio_theme::Controller> m_controller;
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
