#include "RobloxModLoader/qt/qcolor.hpp"

namespace rml::qt
{
	static constexpr std::uint16_t CHANNEL_SCALE = 0x101;

	static constexpr bool is_channel(const int value)
	{
		return value >= 0 && value <= 255;
	}

	QColor::QColor(const int red, const int green, const int blue, const int alpha) noexcept
	{
		if (!is_channel(red) || !is_channel(green) || !is_channel(blue) || !is_channel(alpha))
			return;

		cspec = Rgb;
		ct.argb = {static_cast<std::uint16_t>(alpha * CHANNEL_SCALE), static_cast<std::uint16_t>(red * CHANNEL_SCALE),
		    static_cast<std::uint16_t>(green * CHANNEL_SCALE), static_cast<std::uint16_t>(blue * CHANNEL_SCALE), 0};
	}
}
