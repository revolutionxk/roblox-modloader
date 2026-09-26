#include "RobloxModLoader/util/string.hpp"

#include "RobloxModLoader/internal/common.hpp"

namespace rml::utils
{
	std::wstring to_wide(const std::string_view utf8)
	{
		if (utf8.empty())
			return {};

#if defined(RML_WINDOWS)
		const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
		std::wstring wide(static_cast<std::size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), length);
		return wide;
#else
		return {utf8.begin(), utf8.end()};
#endif
	}
}
