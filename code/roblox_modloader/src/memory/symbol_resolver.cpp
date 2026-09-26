#include "RobloxModLoader/memory/symbol_resolver.hpp"

#include "RobloxModLoader/internal/platform.hpp"

#if defined(RML_WINDOWS)
	#include <windows.h>

	#include <dbghelp.h>
	#pragma comment(lib, "dbghelp.lib")
#else
	#include <cstdlib>
	#include <cxxabi.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <span>
#include <string_view>
#include <vector>

namespace rml::memory
{
	static bool valid_identifiers(const std::span<const std::string_view> parts)
	{
		return std::ranges::all_of(parts, [](const std::string_view part) {
			return !part.empty() && std::ranges::all_of(part, [](const unsigned char c) { return std::isalnum(c) || c == '_'; });
		});
	}

	static bool is_itanium_mangled(const char* mangled)
	{
		return mangled[0] == '_' && mangled[1] == 'Z';
	}

	[[maybe_unused]] static std::string msvc_qualified_name(const char* mangled)
	{
		if (!mangled || mangled[0] != '?' || mangled[1] == '?')
			return {};

		std::vector<std::string_view> parts;
		const char* start = mangled + 1;
		const char* p = start;

		for (; *p; ++p)
		{
			if (p[0] == '@' && p[1] == '@')
				break;

			if (*p == '@')
			{
				parts.emplace_back(start, static_cast<std::size_t>(p - start));
				start = p + 1;
			}
		}

		if (p == start || p[0] != '@' || p[1] != '@')
			return {};

		parts.emplace_back(start, static_cast<std::size_t>(p - start));

		if (parts.size() < 2)
			return {};

		if (!valid_identifiers(parts))
			return {};

		std::string result;
		for (std::size_t i = parts.size(); i-- > 1;)
		{
			result += parts[i];
			result += "::";
		}
		result += parts.front();
		return result;
	}

	[[maybe_unused]] static std::string msvc_vtable_name(const char* mangled)
	{
		if (!mangled || mangled[0] != '?' || mangled[1] != '?' || mangled[2] != '_' || mangled[3] != '7')
			return {};

		std::vector<std::string_view> parts;
		const char* start = mangled + 4;
		const char* p = start;

		for (; *p; ++p)
		{
			if (p[0] == '@' && p[1] == '@')
				break;

			if (*p == '@')
			{
				parts.emplace_back(start, static_cast<std::size_t>(p - start));
				start = p + 1;
			}
		}

		if (p == start || p[0] != '@' || p[1] != '@')
			return {};

		parts.emplace_back(start, static_cast<std::size_t>(p - start));

		if (!valid_identifiers(parts))
			return {};

		std::string result = "vtable for ";
		for (std::size_t i = parts.size(); i-- > 0;)
		{
			result += parts[i];
			if (i != 0)
				result += "::";
		}
		return result;
	}

	std::string demangle(const char* mangled)
	{
		if (!mangled || !*mangled)
		{
			return {};
		}

#if defined(RML_WINDOWS)
		if (mangled[0] != '?')
		{
			return mangled;
		}

		char buffer[2048];
		constexpr DWORD flags = UNDNAME_NAME_ONLY | UNDNAME_NO_ARGUMENTS | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_LEADING_UNDERSCORES;
		const DWORD written = UnDecorateSymbolName(mangled, buffer, sizeof(buffer), flags);
		return written != 0 ? std::string{buffer, written} : std::string{};
#else
		if (!is_itanium_mangled(mangled))
		{
			return mangled;
		}

		std::string demangled = demangle_signature(mangled);

		if (const auto paren = demangled.find('('); paren != std::string::npos)
		{
			demangled.resize(paren);
		}
		return demangled;
#endif
	}

	std::string demangle_signature(const char* mangled)
	{
		if (!mangled || !*mangled)
		{
			return {};
		}

#if defined(RML_WINDOWS)
		if (mangled[0] != '?')
		{
			return mangled;
		}

		if (std::string vtable = msvc_vtable_name(mangled); !vtable.empty())
		{
			return vtable;
		}

		char buffer[4096];
		constexpr DWORD flags = UNDNAME_NO_FUNCTION_RETURNS | UNDNAME_NO_ACCESS_SPECIFIERS
		                      | UNDNAME_NO_MS_KEYWORDS | UNDNAME_NO_MEMBER_TYPE
		                      | UNDNAME_NO_LEADING_UNDERSCORES | UNDNAME_NO_THROW_SIGNATURES;
		const DWORD written = UnDecorateSymbolName(mangled, buffer, sizeof(buffer), flags);

		if (written != 0)
		{
			std::string demangled{buffer, written};
			if (demangled.find('(') != std::string::npos)
			{
				return demangled;
			}
		}

		if (std::string qualified = msvc_qualified_name(mangled); !qualified.empty())
		{
			return qualified;
		}

		return demangle(mangled);
#else
		if (!is_itanium_mangled(mangled))
		{
			return mangled;
		}

		int status = 0;
		char* result = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
		if (status != 0 || !result)
		{
			std::free(result);
			return {};
		}

		std::string demangled{result};
		std::free(result);
		return demangled;
#endif
	}

	std::string normalize_signature(std::string_view demangled)
	{
		static constexpr std::array<std::string_view, 5> noise{"class ", "struct ", "enum ", "union ", "__ptr64"};

		std::string result;
		result.reserve(demangled.size());

		for (std::size_t i = 0; i < demangled.size();)
		{
			const std::string_view rest = demangled.substr(i);

			if (rest.starts_with("(void)"))
			{
				result += "()";
				i += 6;
				continue;
			}

			bool skipped = false;
			for (const auto word : noise)
			{
				if (rest.starts_with(word))
				{
					i += word.size();
					skipped = true;
					break;
				}
			}
			if (skipped)
				continue;

			if (!std::isspace(static_cast<unsigned char>(demangled[i])))
				result += demangled[i];

			++i;
		}

		return result;
	}
}
