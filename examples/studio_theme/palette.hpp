#pragma once

#include "color.hpp"
#include "theme_definition.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace studio_theme
{
	class Palette
	{
	public:
		explicit Palette(ThemeDefinition theme);

		[[nodiscard]] Color map(const Color& stock) const;
		[[nodiscard]] Color map_preset(std::string_view key, const Color& stock) const;
		[[nodiscard]] std::string map_hex(std::string_view hex) const;
		[[nodiscard]] std::string map_preset_hex(std::string_view key, std::string_view hex) const;

		[[nodiscard]] const ThemeDefinition& theme() const
		{
			return m_theme;
		}

	private:
		struct RampKnot
		{
			double stock_lightness{};
			Lab target;
		};

		struct HueAnchor
		{
			Lch stock;
			Lch target;
		};

		[[nodiscard]] const Color* overridden(const Color& stock) const;
		[[nodiscard]] Lab ramp(double lightness) const;
		[[nodiscard]] const HueAnchor* nearest_hue(double hue) const;

		ThemeDefinition m_theme;
		std::vector<RampKnot> m_ramp;
		std::vector<HueAnchor> m_hues;
	};
}
