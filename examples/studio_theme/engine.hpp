#pragma once

#include <RobloxModLoader/qt/qbytearray.hpp>
#include <RobloxModLoader/roblox/studio/theme.hpp>

namespace studio_theme
{
	struct Engine
	{
		void (*parse_theme_json)(Theme* self, const rml::qt::QByteArray& json){};
		void (*set_current_theme)(CoreStudioSettings* self, const Theme& theme, bool request_change, bool persist){};
		CoreStudioSettings* (*core_studio_settings)(){};

		[[nodiscard]] bool complete() const
		{
			return parse_theme_json && set_current_theme && core_studio_settings;
		}
	};

	[[nodiscard]] Engine resolve_engine();
}
