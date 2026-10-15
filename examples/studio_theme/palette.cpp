#include "palette.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <utility>

namespace studio_theme
{
	struct StockAnchors
	{
		std::array<std::string_view, 5> neutrals;
		std::array<std::pair<std::string_view, Role>, 5> hues;
	};

	static constexpr StockAnchors k_dark_anchors{
	    {"#191A1F", "#272930", "#353741", "#6A6F81", "#BCBEC8"},
	    {{{"#E75750", Role::Error}, {"#F57630", Role::Warning}, {"#F2BA2A", Role::Warning}, {"#1DB16A", Role::Success}, {"#335FFF", Role::Accent}}}};

	static constexpr StockAnchors k_light_anchors{
	    {"#FFFFFF", "#EEEFF1", "#D5D7DD", "#6A6F81", "#191A1F"},
	    {{{"#DF281F", Role::Error}, {"#F57630", Role::Warning}, {"#DBA61F", Role::Warning}, {"#0C9B5A", Role::Success}, {"#335FFF", Role::Accent}}}};

	static constexpr std::array<Role, 5> k_neutral_roles{Role::Background, Role::Surface, Role::Border, Role::Muted, Role::Text};
	static constexpr double k_neutral_chroma = 0.035;
	static constexpr double k_untinted_chroma = 0.015;
	static constexpr double k_hue_reach = 30.0;

	static bool neutral(const Lch& color)
	{
		return color.c < k_neutral_chroma && (color.c < k_untinted_chroma || (color.h >= 240.0 && color.h <= 300.0));
	}

	static double hue_delta(const double from, const double to)
	{
		double delta = std::fmod(to - from, 360.0);
		if (delta > 180.0)
			delta -= 360.0;
		if (delta < -180.0)
			delta += 360.0;
		return delta;
	}

	static Lab stock_lab(const std::string_view hex)
	{
		return to_oklab(parse_hex(hex).value_or(Color{}));
	}

	Palette::Palette(ThemeDefinition theme) :
	    m_theme(std::move(theme))
	{
		const auto& anchors = m_theme.base == Base::Light ? k_light_anchors : k_dark_anchors;

		for (std::size_t i = 0; i < anchors.neutrals.size(); ++i)
			m_ramp.push_back({stock_lab(anchors.neutrals[i]).l, to_oklab(m_theme.role(k_neutral_roles[i]))});
		std::ranges::sort(m_ramp, {}, &RampKnot::stock_lightness);

		for (const auto& [hex, role] : anchors.hues)
			m_hues.push_back({to_lch(stock_lab(hex)), to_lch(to_oklab(m_theme.role(role)))});
	}

	const Color* Palette::overridden(const Color& stock) const
	{
		const auto it = m_theme.overrides.find(rgb_key(stock));
		return it == m_theme.overrides.end() ? nullptr : &it->second;
	}

	Lab Palette::ramp(const double lightness) const
	{
		const auto& first = m_ramp.front();
		const auto& last = m_ramp.back();
		if (lightness <= first.stock_lightness)
			return {first.target.l + (lightness - first.stock_lightness), first.target.a, first.target.b};
		if (lightness >= last.stock_lightness)
			return {last.target.l + (lightness - last.stock_lightness), last.target.a, last.target.b};

		const auto upper = std::ranges::upper_bound(m_ramp, lightness, {}, &RampKnot::stock_lightness);
		const auto& high = *upper;
		const auto& low = *std::prev(upper);
		const double t = (lightness - low.stock_lightness) / (high.stock_lightness - low.stock_lightness);
		return {std::lerp(low.target.l, high.target.l, t), std::lerp(low.target.a, high.target.a, t), std::lerp(low.target.b, high.target.b, t)};
	}

	const Palette::HueAnchor* Palette::nearest_hue(const double hue) const
	{
		const HueAnchor* nearest = nullptr;
		double distance = k_hue_reach;
		for (const auto& anchor : m_hues)
		{
			if (const double gap = std::abs(hue_delta(anchor.stock.h, hue)); gap <= distance)
			{
				nearest = &anchor;
				distance = gap;
			}
		}
		return nearest;
	}

	Color Palette::map(const Color& stock) const
	{
		if (const auto* replacement = overridden(stock))
			return {replacement->r, replacement->g, replacement->b, stock.a};

		const auto lab = to_oklab(stock);
		const auto lch = to_lch(lab);
		if (neutral(lch))
			return from_oklab(ramp(lab.l), stock.a);

		if (const auto* anchor = nearest_hue(lch.h))
		{
			const double chroma = anchor->stock.c > 0.0 ? anchor->target.c * lch.c / anchor->stock.c : anchor->target.c;
			const Lch shifted{anchor->target.l + (lch.l - anchor->stock.l), chroma, anchor->target.h + hue_delta(anchor->stock.h, lch.h)};
			return from_oklab(to_lab(shifted), stock.a);
		}
		return stock;
	}

	Color Palette::map_preset(const std::string_view key, const Color& stock) const
	{
		if (const auto it = m_theme.syntax.find(std::string{key}); it != m_theme.syntax.end())
			return {it->second.r, it->second.g, it->second.b, stock.a};
		if (overridden(stock) || neutral(to_lch(to_oklab(stock))))
			return map(stock);
		return stock;
	}

	std::string Palette::map_hex(const std::string_view hex) const
	{
		const auto stock = parse_hex(hex);
		return stock ? to_hex(map(*stock), hex.size() == 9) : std::string{hex};
	}

	std::string Palette::map_preset_hex(const std::string_view key, const std::string_view hex) const
	{
		const auto stock = parse_hex(hex);
		return stock ? to_hex(map_preset(key, *stock), hex.size() == 9) : std::string{hex};
	}
}
