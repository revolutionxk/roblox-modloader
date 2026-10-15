#pragma once

#include "color.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace studio_theme
{
	enum class Base : std::uint8_t
	{
		Dark,
		Light,
	};

	enum class Role : std::uint8_t
	{
		Background,
		Surface,
		Border,
		Muted,
		Text,
		Accent,
		Success,
		Warning,
		Error,
		Count,
	};

	inline constexpr std::array<std::string_view, static_cast<std::size_t>(Role::Count)> k_role_names{
	    "background", "surface", "border", "muted", "text", "accent", "success", "warning", "error"};

	struct ThemeDefinition
	{
		std::string name;
		Base base{Base::Dark};
		std::array<Color, static_cast<std::size_t>(Role::Count)> roles{};
		std::unordered_map<std::string, Color> syntax;
		std::unordered_map<std::uint32_t, Color> overrides;
		bool builtin{};

		[[nodiscard]] const Color& role(const Role which) const
		{
			return roles[static_cast<std::size_t>(which)];
		}
	};

	[[nodiscard]] constexpr std::string_view base_name(const Base base)
	{
		return base == Base::Light ? "Light" : "Dark";
	}
}
