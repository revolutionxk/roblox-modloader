#pragma once

#include <RobloxModLoader/config/mod_settings.hpp>
#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <string_view>

namespace script_editor_bg
{
	enum class ScaleMode
	{
		Stretch,
		Fit,
		Fill,
		Center,
		Tile,
	};

	enum class Alignment
	{
		Center,
		TopLeft,
		Top,
		TopRight,
		Left,
		Right,
		BottomLeft,
		Bottom,
		BottomRight,
	};

	struct BackgroundSettings
	{
		bool enabled{false};
		double opacity{0.22};
		double blur{0.0};
		std::string image{""};
		ScaleMode scale_mode{ScaleMode::Fill};
		Alignment alignment{Alignment::Center};
	};

	namespace keys
	{
		inline constexpr std::string_view enabled = "enabled";
		inline constexpr std::string_view opacity = "opacity";
		inline constexpr std::string_view blur = "blur";
		inline constexpr std::string_view image = "image";
		inline constexpr std::string_view scale_mode = "scale_mode";
		inline constexpr std::string_view alignment = "alignment";
	}

	inline constexpr double min_opacity = 0.0;
	inline constexpr double max_opacity = 1.0;
	inline constexpr double opacity_step = 0.05;

	inline constexpr double min_blur = 0.0;
	inline constexpr double max_blur = 1.0;

	[[nodiscard]] inline double clamp_opacity(const double value)
	{
		return std::clamp(value, min_opacity, max_opacity);
	}

	[[nodiscard]] inline double clamp_blur(const double value)
	{
		return std::clamp(value, min_blur, max_blur);
	}

	[[nodiscard]] inline std::string_view to_string(const ScaleMode mode)
	{
		switch (mode)
		{
		case ScaleMode::Stretch: return "stretch";
		case ScaleMode::Fit: return "fit";
		case ScaleMode::Fill: return "fill";
		case ScaleMode::Center: return "center";
		case ScaleMode::Tile: return "tile";
		}
		return "fill";
	}

	[[nodiscard]] inline ScaleMode scale_mode_from_string(const std::string_view text)
	{
		if (text == "stretch")
			return ScaleMode::Stretch;
		if (text == "fit")
			return ScaleMode::Fit;
		if (text == "center")
			return ScaleMode::Center;
		if (text == "tile")
			return ScaleMode::Tile;
		return ScaleMode::Fill;
	}

	template<class T, std::size_t N>
	[[nodiscard]] constexpr T next_in(const std::array<T, N>& order, const T current)
	{
		const auto it = std::ranges::find(order, current);
		return it == order.end() || std::next(it) == order.end() ? order.front() : *std::next(it);
	}

	[[nodiscard]] inline ScaleMode next_scale_mode(const ScaleMode mode)
	{
		constexpr std::array order{ScaleMode::Fill, ScaleMode::Fit, ScaleMode::Stretch, ScaleMode::Center, ScaleMode::Tile};
		return next_in(order, mode);
	}

	[[nodiscard]] inline Alignment next_alignment(const Alignment alignment)
	{
		constexpr std::array order{Alignment::Center, Alignment::TopLeft, Alignment::Top, Alignment::TopRight, Alignment::Left, Alignment::Right, Alignment::BottomLeft, Alignment::Bottom, Alignment::BottomRight};
		return next_in(order, alignment);
	}

	[[nodiscard]] inline std::string_view to_string(const Alignment alignment)
	{
		switch (alignment)
		{
		case Alignment::Center: return "center";
		case Alignment::TopLeft: return "top-left";
		case Alignment::Top: return "top";
		case Alignment::TopRight: return "top-right";
		case Alignment::Left: return "left";
		case Alignment::Right: return "right";
		case Alignment::BottomLeft: return "bottom-left";
		case Alignment::Bottom: return "bottom";
		case Alignment::BottomRight: return "bottom-right";
		}
		return "center";
	}

	[[nodiscard]] inline Alignment alignment_from_string(const std::string_view text)
	{
		if (text == "top-left")
			return Alignment::TopLeft;
		if (text == "top")
			return Alignment::Top;
		if (text == "top-right")
			return Alignment::TopRight;
		if (text == "left")
			return Alignment::Left;
		if (text == "right")
			return Alignment::Right;
		if (text == "bottom-left")
			return Alignment::BottomLeft;
		if (text == "bottom")
			return Alignment::Bottom;
		if (text == "bottom-right")
			return Alignment::BottomRight;
		return Alignment::Center;
	}

	[[nodiscard]] inline std::string default_config_template()
	{
		const BackgroundSettings defaults{};
		std::string out;
		out += "enabled = ";
		out += defaults.enabled ? "true" : "false";
		out += "\n";
		out += "opacity = 0.22\n";
		out += "blur = 0.0\n";
		out += "image = \"\"\n";
		out += "scale_mode = \"fill\"\n";
		out += "alignment = \"center\"\n";
		return out;
	}

	[[nodiscard]] inline BackgroundSettings read(const rml::config::ModSettings& store)
	{
		BackgroundSettings settings;
		settings.enabled = store.get_bool(keys::enabled, settings.enabled);
		settings.opacity = clamp_opacity(store.get_double(keys::opacity, settings.opacity));
		settings.blur = clamp_blur(store.get_double(keys::blur, settings.blur));
		settings.image = store.get_string(keys::image, settings.image);
		settings.scale_mode = scale_mode_from_string(store.get_string(keys::scale_mode, to_string(settings.scale_mode)));
		settings.alignment = alignment_from_string(store.get_string(keys::alignment, to_string(settings.alignment)));
		return settings;
	}

	inline void write(const rml::config::ModSettings& store, const BackgroundSettings& settings)
	{
		store.set_bool(keys::enabled, settings.enabled);
		store.set_double(keys::opacity, clamp_opacity(settings.opacity));
		store.set_double(keys::blur, clamp_blur(settings.blur));
		store.set_string(keys::image, settings.image);
		store.set_string(keys::scale_mode, to_string(settings.scale_mode));
		store.set_string(keys::alignment, to_string(settings.alignment));
	}
}
