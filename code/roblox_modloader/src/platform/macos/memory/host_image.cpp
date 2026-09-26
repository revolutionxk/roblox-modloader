#include "RobloxModLoader/platform/memory/host_image.hpp"
#include "RobloxModLoader/memory/module.hpp"

#include <cstring>

#include <dlfcn.h>
#include <limits.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace rml::platform
{
	std::filesystem::path module_path_containing(const void* address)
	{
		Dl_info info{};

		if (!dladdr(address, &info) || !info.dli_fname)
			return {};

		return info.dli_fname;
	}

	std::filesystem::path executable_path()
	{
		char exe_path[PATH_MAX];
		auto size = static_cast<std::uint32_t>(sizeof(exe_path));

		if (_NSGetExecutablePath(exe_path, &size) != 0)
			return {};

		return exe_path;
	}

	std::string_view studio_image_name()
	{
		return "RobloxStudio";
	}

	std::uintptr_t studio_preferred_image_base()
	{
		return 0x100000000;
	}

	void* acquire_main_window()
	{
		return nullptr;
	}

	ImageIdentity studio_image_identity()
	{
		ImageIdentity id{};
		const memory::module image(studio_image_name());
		const auto base = image.begin().as<std::uintptr_t>();
		if (!base)
			return id;

		const auto* header = reinterpret_cast<const mach_header_64*>(base);
		if (header->magic != MH_MAGIC_64)
			return id;

		const auto* command = reinterpret_cast<const load_command*>(header + 1);
		for (std::uint32_t i = 0; i < header->ncmds; ++i)
		{
			if (command->cmd == LC_UUID)
			{
				std::memcpy(id.stamp.data(), reinterpret_cast<const uuid_command*>(command)->uuid, id.stamp.size());
				id.image_size = static_cast<std::uint32_t>(image.size());
				break;
			}
			command = reinterpret_cast<const load_command*>(reinterpret_cast<const std::byte*>(command) + command->cmdsize);
		}
		return id;
	}
}
