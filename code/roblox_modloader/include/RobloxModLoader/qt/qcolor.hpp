#pragma once

#include "RobloxModLoader/rml_export.hpp"
#include "RobloxModLoader/util/layout_assert.hpp"

#include <cstdint>

namespace rml::qt
{
	class RML_EXPORT QColor
	{
	public:
		enum Spec : int
		{
			Invalid,
			Rgb,
			Hsv,
			Cmyk,
			Hsl,
			ExtendedRgb,
		};

		struct Argb
		{
			std::uint16_t alpha;
			std::uint16_t red;
			std::uint16_t green;
			std::uint16_t blue;
			std::uint16_t pad;
		};

		union Components
		{
			Argb argb;
			std::uint16_t array[5];
		};

		Spec cspec{Invalid};
		Components ct{};

		constexpr QColor() noexcept = default;
		QColor(int red, int green, int blue, int alpha = 255) noexcept;
	};

	RML_LAYOUT_DIAGNOSTIC_PUSH()
	RML_ASSERT_OFFSET(QColor, ct, 4);
	RML_ASSERT_SIZE(QColor, 16);
	RML_LAYOUT_DIAGNOSTIC_POP()
}
