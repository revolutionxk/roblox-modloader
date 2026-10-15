#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace studio_theme
{
	struct Color
	{
		double r{};
		double g{};
		double b{};
		double a{1.0};
	};

	struct Lab
	{
		double l{};
		double a{};
		double b{};
	};

	struct Lch
	{
		double l{};
		double c{};
		double h{};
	};

	[[nodiscard]] std::optional<Color> parse_hex(std::string_view text);
	[[nodiscard]] std::string to_hex(const Color& color, bool with_alpha);
	[[nodiscard]] std::uint32_t rgb_key(const Color& color);
	[[nodiscard]] Lab to_oklab(const Color& color);
	[[nodiscard]] Color from_oklab(Lab lab, double alpha);
	[[nodiscard]] Lch to_lch(const Lab& lab);
	[[nodiscard]] Lab to_lab(const Lch& lch);
}
