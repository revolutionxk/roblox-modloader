#pragma once

#include <string>
#include <string_view>

namespace rml::utils
{
	inline void replace_all(std::string& text, const std::string_view from, const std::string_view to)
	{
		for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size()))
			text.replace(at, from.size(), to);
	}

	[[nodiscard]] inline std::string canonical_type_name(const std::string_view name)
	{
		std::string out;
		out.reserve(name.size());
		for (const char c : name)
		{
			if (c != ' ')
				out += c;
		}

		for (const std::string_view keyword : {"class", "struct", "enum"})
		{
			if (out.starts_with(keyword))
				out.erase(0, keyword.size());

			for (const char lead : {'<', ',', '('})
			{
				std::string from(1, lead);
				from += keyword;
				replace_all(out, from, std::string(1, lead));
			}
		}

		replace_all(out, "std::__1::", "std::");
		replace_all(out, "__cdecl", "");
		replace_all(out, "__ptr64", "");
		replace_all(out, "(void)", "()");
		replace_all(out, "std::basic_string<char,std::char_traits<char>,std::allocator<char>>", "std::string");
		return out;
	}

	[[nodiscard]] constexpr bool glob_match(const std::string_view pattern, const std::string_view text)
	{
		std::size_t p = 0;
		std::size_t t = 0;
		std::size_t star = std::string_view::npos;
		std::size_t mark = 0;

		while (t < text.size())
		{
			if (p < pattern.size() && pattern[p] == '*')
			{
				star = p++;
				mark = t;
			}
			else if (p < pattern.size() && pattern[p] == text[t])
			{
				++p;
				++t;
			}
			else if (star != std::string_view::npos)
			{
				p = star + 1;
				t = ++mark;
			}
			else
			{
				return false;
			}
		}

		while (p < pattern.size() && pattern[p] == '*')
			++p;
		return p == pattern.size();
	}
}
