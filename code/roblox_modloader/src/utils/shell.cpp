#include "shell.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/util/string.hpp"

#include <cstdlib>

#if defined(RML_WINDOWS)
	#include <shellapi.h>
	#pragma comment(lib, "shell32.lib")
	#pragma comment(lib, "user32.lib")
#endif

namespace rml::utils
{
	void shell::open(const std::filesystem::path& path)
	{
#if defined(RML_WINDOWS)
		ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
		std::system(("xdg-open \"" + path.string() + "\" >/dev/null 2>&1 &").c_str());
#endif
	}

	void shell::open_folder(const std::filesystem::path& path)
	{
		std::error_code ec;
		std::filesystem::create_directories(path, ec);
		open(path);
	}

	void shell::message_box(const std::string_view title, const std::string_view text)
	{
#if defined(RML_WINDOWS)
		MessageBoxW(nullptr, to_wide(text).c_str(), to_wide(title).c_str(), MB_OK | MB_ICONINFORMATION);
#else
		(void)title;
		(void)text;
#endif
	}
}
