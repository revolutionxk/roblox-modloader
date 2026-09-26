#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/memory/rtti_index.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

#include <dbghelp.h>

#include <cstring>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace rml::memory
{
	struct CompleteObjectLocator
	{
		std::uint32_t signature;
		std::uint32_t offset;
		std::uint32_t constructor_displacement;
		std::uint32_t type_descriptor;
		std::uint32_t class_hierarchy;
		std::uint32_t self;
	};

	struct TypeDescriptor
	{
		const void* vtable;
		void* spare;
		char name[1];
	};

	struct SectionRange
	{
		std::uintptr_t begin{};
		std::uintptr_t end{};

		[[nodiscard]] bool contains(const std::uintptr_t address, const std::size_t size = 1) const noexcept
		{
			return address >= begin && address < end && end - address >= size;
		}
	};

	class MsvcRttiBackend final : public IRttiBackend
	{
	public:
		void enumerate(const Sink sink, void* context) const override
		{
			const module image(platform::studio_image_name());
			const auto base = image.begin().as<std::uintptr_t>();
			if (!base)
				return;

			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);

			SectionRange rdata;
			SectionRange data;
			const auto* section = IMAGE_FIRST_SECTION(nt);
			for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
			{
				const std::string_view name(reinterpret_cast<const char*>(section->Name), strnlen(reinterpret_cast<const char*>(section->Name), IMAGE_SIZEOF_SHORT_NAME));
				const SectionRange range{base + section->VirtualAddress, base + section->VirtualAddress + section->Misc.VirtualSize};
				if (name == ".rdata")
					rdata = range;
				else if (name == ".data")
					data = range;
			}

			const auto* slots = reinterpret_cast<const std::uintptr_t*>(rdata.begin);
			const std::size_t count = (rdata.end - rdata.begin) / sizeof(std::uintptr_t);
			for (std::size_t i = 0; i + 1 < count; ++i)
			{
				const auto candidate = slots[i];
				if (!rdata.contains(candidate, sizeof(CompleteObjectLocator)))
					continue;

				const auto* col = reinterpret_cast<const CompleteObjectLocator*>(candidate);
				if (col->signature != 1 || col->offset != 0 || base + col->self != candidate)
					continue;

				const auto descriptor_address = base + col->type_descriptor;
				if (!data.contains(descriptor_address, sizeof(TypeDescriptor)))
					continue;

				const auto* descriptor = reinterpret_cast<const TypeDescriptor*>(descriptor_address);
				if (std::strncmp(descriptor->name, ".?A", 3) != 0)
					continue;

				const auto name = undecorate(descriptor->name);
				if (!name.empty())
					sink(context, name, reinterpret_cast<void**>(const_cast<std::uintptr_t*>(slots + i + 1)));
			}
		}

	private:
		static std::string undecorate(const char* type_name)
		{
			std::string decorated = "??_R0";
			decorated += type_name + 1;
			decorated += "@8";

			char buffer[4096];
			const auto written = UnDecorateSymbolName(decorated.c_str(), buffer, sizeof(buffer), UNDNAME_NAME_ONLY | UNDNAME_NO_MS_KEYWORDS);
			if (written == 0)
				return {};

			std::string_view text(buffer, written);
			if (const auto suffix = text.rfind(" `RTTI Type Descriptor'"); suffix != std::string_view::npos)
				text = text.substr(0, suffix);
			return std::string(text);
		}
	};

	std::unique_ptr<IRttiBackend> create_rtti_backend()
	{
		return std::make_unique<MsvcRttiBackend>();
	}
}
