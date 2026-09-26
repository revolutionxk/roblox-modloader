#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/memory/module.hpp"

#include <cstring>

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

	void* acquire_main_window()
	{
		void* window = GetForegroundWindow();

		if (!window)
			throw std::runtime_error("Failed to find Roblox Studio window");

		return window;
	}

	ImageIdentity studio_image_identity()
	{
		ImageIdentity id{};
		const memory::module image(studio_image_name());
		const auto base = image.begin().as<std::uintptr_t>();
		if (!base)
			return id;

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return id;

		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return id;

		std::memcpy(id.stamp.data(), &nt->FileHeader.TimeDateStamp, sizeof(nt->FileHeader.TimeDateStamp));
		id.image_size = nt->OptionalHeader.SizeOfImage;
		return id;
	}
}
