#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

#include <cxxabi.h>
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace rml::memory
{
	class ItaniumRttiBackend final : public IRttiBackend
	{
	public:
		void enumerate(const Sink sink, void* context) const override
		{
			const module image(platform::studio_image_name());
			const auto begin = image.begin().as<std::uintptr_t>();
			const auto end = begin + image.size();
			if (!begin)
				return;

			const auto type_info_vtables = class_type_info_vtables();
			if (std::ranges::all_of(type_info_vtables, [](const std::uintptr_t vtable) { return vtable == 0; }))
				return;

			const auto is_class_type_info = [&](const std::uintptr_t vptr) {
				return vptr != 0 && std::ranges::find(type_info_vtables, vptr) != type_info_vtables.end();
			};
			const auto in_image = [&](const std::uintptr_t address) {
				return address >= begin && address < end && end - address >= sizeof(std::uintptr_t);
			};

			const auto* header = reinterpret_cast<const mach_header_64*>(begin);
			unsigned long text_size = 0;
			const auto text = reinterpret_cast<std::uintptr_t>(getsectiondata(header, "__TEXT", "__text", &text_size));
			const auto pure_virtual = reinterpret_cast<std::uintptr_t>(dlsym(RTLD_DEFAULT, "__cxa_pure_virtual"));
			const auto is_first_virtual = [&](const std::uintptr_t function) {
				return (function >= text && function - text < text_size) || (pure_virtual && function == pure_virtual);
			};
			for (const auto& [segment, section] : k_vtable_sections)
			{
				unsigned long size = 0;
				const auto* data = getsectiondata(header, segment, section, &size);
				if (!data)
					continue;

				const auto* slots = reinterpret_cast<const std::uintptr_t*>(data);
				const std::size_t count = size / sizeof(std::uintptr_t);
				for (std::size_t i = 0; i + 2 < count; ++i)
				{
					if (slots[i] != 0 || !in_image(slots[i + 1]))
						continue;

					const auto* type_info = reinterpret_cast<const std::uintptr_t*>(slots[i + 1]);
					const auto name = type_info[1] & ~k_non_unique_name_bit;
					if (!is_class_type_info(type_info[0]) || !in_image(name) || !is_first_virtual(slots[i + 2]))
						continue;

					const auto* mangled = reinterpret_cast<const char*>(name);
					int status = 0;
					char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
					if (status == 0 && demangled)
						sink(context, demangled, reinterpret_cast<void**>(const_cast<std::uintptr_t*>(slots + i + 2)));
					std::free(demangled);
				}
			}
		}

	private:
		static constexpr std::uintptr_t k_non_unique_name_bit = std::uintptr_t{1} << 63;
		static constexpr std::array<std::pair<const char*, const char*>, 2> k_vtable_sections{{{"__DATA_CONST", "__const"}, {"__DATA", "__const"}}};

		static std::array<std::uintptr_t, 3> class_type_info_vtables()
		{
			constexpr std::array symbols{"_ZTVN10__cxxabiv117__class_type_infoE", "_ZTVN10__cxxabiv120__si_class_type_infoE", "_ZTVN10__cxxabiv121__vmi_class_type_infoE"};

			std::array<std::uintptr_t, 3> vtables{};
			for (std::size_t i = 0; i < symbols.size(); ++i)
			{
				if (const auto* vtable = static_cast<const std::uintptr_t*>(dlsym(RTLD_DEFAULT, symbols[i])))
					vtables[i] = reinterpret_cast<std::uintptr_t>(vtable + 2);
			}
			return vtables;
		}
	};

	std::unique_ptr<IRttiBackend> create_rtti_backend()
	{
		return std::make_unique<ItaniumRttiBackend>();
	}
}
