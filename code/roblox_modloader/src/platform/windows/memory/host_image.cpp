#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

namespace rml::platform
{
	std::filesystem::path module_path_containing(const void* address)
	{
		HMODULE module_handle = nullptr;

		if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		        reinterpret_cast<LPCWSTR>(address), &module_handle))
			return {};

		wchar_t module_path[MAX_PATH];
		if (GetModuleFileNameW(module_handle, module_path, MAX_PATH) == 0)
			return {};

		return module_path;
	}

	std::filesystem::path executable_path()
	{
		wchar_t exe_path[MAX_PATH];

		if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) == 0)
			return {};

		return exe_path;
	}

	std::string_view studio_image_name()
	{
		return "RobloxStudioBeta.exe";
	}

	std::uintptr_t studio_preferred_image_base()
	{
		return 0x140000000;
	}
}
