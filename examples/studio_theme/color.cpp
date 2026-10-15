#include "color.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <format>
#include <numbers>

namespace studio_theme
{
	static unsigned to_byte(const double channel)
	{
		return static_cast<unsigned>(std::lround(std::clamp(channel, 0.0, 1.0) * 255.0));
	}

	static double to_linear(const double channel)
	{
		return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
	}

	static double to_gamma(const double channel)
	{
		return channel <= 0.0031308 ? channel * 12.92 : 1.055 * std::pow(channel, 1.0 / 2.4) - 0.055;
	}

	static std::array<double, 3> linear_rgb(const Lab& lab)
	{
		const double l = lab.l + 0.3963377774 * lab.a + 0.2158037573 * lab.b;
		const double m = lab.l - 0.1055613458 * lab.a - 0.0638541728 * lab.b;
		const double s = lab.l - 0.0894841775 * lab.a - 1.2914855480 * lab.b;
		const double l3 = l * l * l;
		const double m3 = m * m * m;
		const double s3 = s * s * s;
		return {4.0767416621 * l3 - 3.3077115913 * m3 + 0.2309699292 * s3,
		    -1.2684380046 * l3 + 2.6097574011 * m3 - 0.3413193965 * s3,
		    -0.0041960863 * l3 - 0.7034186147 * m3 + 1.7076147010 * s3};
	}

	static bool in_gamut(const std::array<double, 3>& rgb)
	{
		return std::ranges::all_of(rgb, [](const double channel) {
			return channel >= -1e-4 && channel <= 1.0 + 1e-4;
		});
	}

	std::optional<Color> parse_hex(const std::string_view text)
	{
		if ((text.size() != 7 && text.size() != 9) || text.front() != '#')
			return std::nullopt;

		std::uint32_t value{};
		const auto* const begin = text.data() + 1;
		const auto* const end = text.data() + text.size();
		if (const auto [last, error] = std::from_chars(begin, end, value, 16); error != std::errc{} || last != end)
			return std::nullopt;

		const auto channel = [value](const int shift) {
			return static_cast<double>((value >> shift) & 0xFF) / 255.0;
		};
		return Color{channel(16), channel(8), channel(0), text.size() == 9 ? channel(24) : 1.0};
	}

	std::string to_hex(const Color& color, const bool with_alpha)
	{
		if (with_alpha)
			return std::format("#{:02X}{:02X}{:02X}{:02X}", to_byte(color.a), to_byte(color.r), to_byte(color.g), to_byte(color.b));
		return std::format("#{:02X}{:02X}{:02X}", to_byte(color.r), to_byte(color.g), to_byte(color.b));
	}

	std::uint32_t rgb_key(const Color& color)
	{
		return (to_byte(color.r) << 16) | (to_byte(color.g) << 8) | to_byte(color.b);
	}

	Lab to_oklab(const Color& color)
	{
		const double r = to_linear(color.r);
		const double g = to_linear(color.g);
		const double b = to_linear(color.b);
		const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
		const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
		const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
		return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
		    1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
		    0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
	}

	Color from_oklab(Lab lab, const double alpha)
	{
		lab.l = std::clamp(lab.l, 0.0, 1.0);
		auto rgb = linear_rgb(lab);
		if (!in_gamut(rgb))
		{
			double inside = 0.0;
			double outside = 1.0;
			for (int step = 0; step < 24; ++step)
			{
				const double scale = (inside + outside) / 2.0;
				if (in_gamut(linear_rgb({lab.l, lab.a * scale, lab.b * scale})))
					inside = scale;
				else
					outside = scale;
			}
			rgb = linear_rgb({lab.l, lab.a * inside, lab.b * inside});
		}

		const auto encode = [](const double channel) {
			return std::clamp(to_gamma(std::clamp(channel, 0.0, 1.0)), 0.0, 1.0);
		};
		return {encode(rgb[0]), encode(rgb[1]), encode(rgb[2]), alpha};
	}

	Lch to_lch(const Lab& lab)
	{
		const double hue = std::atan2(lab.b, lab.a) * 180.0 / std::numbers::pi;
		return {lab.l, std::hypot(lab.a, lab.b), hue < 0.0 ? hue + 360.0 : hue};
	}

	Lab to_lab(const Lch& lch)
	{
		const double radians = lch.h * std::numbers::pi / 180.0;
		return {lch.l, lch.c * std::cos(radians), lch.c * std::sin(radians)};
	}
}
