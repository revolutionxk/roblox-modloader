#pragma once

#include "RobloxModLoader/rml_export.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <ranges>
#include <string>
#include <string_view>

namespace rml::utils
{
	[[nodiscard]] RML_EXPORT std::wstring to_wide(std::string_view utf8);

	[[nodiscard]] inline char to_lower(const char c) noexcept
	{
		return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	[[nodiscard]] inline std::string to_lower(const std::string_view text)
	{
		std::string folded(text.size(), '\0');
		std::ranges::transform(text, folded.begin(), [](const char c) { return to_lower(c); });
		return folded;
	}

	[[nodiscard]] inline bool iequals(const std::string_view a, const std::string_view b) noexcept
	{
		return std::ranges::equal(a, b, [](const char x, const char y) { return to_lower(x) == to_lower(y); });
	}

	template<std::ranges::input_range R>
	[[nodiscard]] std::string join(R&& parts, const std::string_view separator)
	{
		std::string joined;
		bool first = true;
		for (auto&& part : parts)
		{
			if (!first)
				joined += separator;
			joined += std::string_view(part);
			first = false;
		}
		return joined;
	}

	[[nodiscard]] inline bool is_printable(const std::string_view text, const std::size_t max_length) noexcept
	{
		return !text.empty() && text.size() < max_length
		    && std::ranges::all_of(text, [](const unsigned char c) { return std::isprint(c) != 0; });
	}

	[[nodiscard]] inline std::string from_utf16(const std::u16string_view text)
	{
		std::string out;
		out.reserve(text.size());

		const auto append = [&out](const char32_t code_point) {
			if (code_point < 0x80)
			{
				out += static_cast<char>(code_point);
			}
			else if (code_point < 0x800)
			{
				out += static_cast<char>(0xC0 | (code_point >> 6));
				out += static_cast<char>(0x80 | (code_point & 0x3F));
			}
			else if (code_point < 0x10000)
			{
				out += static_cast<char>(0xE0 | (code_point >> 12));
				out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code_point & 0x3F));
			}
			else
			{
				out += static_cast<char>(0xF0 | (code_point >> 18));
				out += static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
				out += static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
				out += static_cast<char>(0x80 | (code_point & 0x3F));
			}
		};

		for (std::size_t i = 0; i < text.size(); ++i)
		{
			const char32_t unit = text[i];
			const bool high = unit >= 0xD800 && unit <= 0xDBFF;
			if (high && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] <= 0xDFFF)
				append(0x10000 + ((unit - 0xD800) << 10) + (text[++i] - 0xDC00));
			else if (unit >= 0xD800 && unit <= 0xDFFF)
				append(0xFFFD);
			else
				append(unit);
		}
		return out;
	}
}
