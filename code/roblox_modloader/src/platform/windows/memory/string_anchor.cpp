#include "memory/string_anchor_image.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/instruction.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace rml::memory
{
	struct ImageLayout
	{
		detail::AddressRange image;
		detail::AddressRange text;
		std::array<detail::AddressRange, 1> strings;
		const IMAGE_RUNTIME_FUNCTION_ENTRY* functions{};
		std::size_t function_count{};
	};

	static ImageLayout read_image_layout()
	{
		ImageLayout layout;
		const module image(platform::studio_image_name());
		const auto base = image.begin().as<std::uintptr_t>();

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		layout.image = {base, base + nt->OptionalHeader.SizeOfImage};

		const auto* section = IMAGE_FIRST_SECTION(nt);
		for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
		{
			const std::string_view name(reinterpret_cast<const char*>(section->Name),
			    strnlen(reinterpret_cast<const char*>(section->Name), IMAGE_SIZEOF_SHORT_NAME));
			const detail::AddressRange range{base + section->VirtualAddress, base + section->VirtualAddress + section->Misc.VirtualSize};
			if (name == ".text")
				layout.text = range;
			else if (name == ".rdata")
				layout.strings[0] = range;
		}

		const auto& exceptions = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
		if (exceptions.VirtualAddress)
		{
			layout.functions = reinterpret_cast<const IMAGE_RUNTIME_FUNCTION_ENTRY*>(base + exceptions.VirtualAddress);
			layout.function_count = exceptions.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
		}

		return layout;
	}

	static const ImageLayout& image_layout()
	{
		static const ImageLayout layout = read_image_layout();
		return layout;
	}

	static std::int32_t read_rel32(const std::uint8_t* code)
	{
		std::int32_t displacement;
		std::memcpy(&displacement, code, sizeof(displacement));
		return displacement;
	}

	static std::optional<std::uintptr_t> rip_reference(const std::uintptr_t address)
	{
		const auto* code = reinterpret_cast<const std::uint8_t*>(address);
		if ((code[0] & 0xF8) != 0x48 || code[1] != 0x8D || (code[2] & 0xC7) != 0x05)
			return std::nullopt;
		return instruction::x64_relative_target(address + 7, read_rel32(code + 3));
	}

	static bool is_relative_branch(const std::uint8_t opcode)
	{
		return opcode == 0xE8 || opcode == 0xE9;
	}

	static bool is_call_target(const ImageLayout& layout, const std::uintptr_t address)
	{
		if (address <= layout.text.begin || address >= layout.text.end)
			return false;

		const auto rva = static_cast<std::uint32_t>(address - layout.image.begin);
		const auto* entry = std::lower_bound(layout.functions, layout.functions + layout.function_count, rva, [](const IMAGE_RUNTIME_FUNCTION_ENTRY& e, const std::uint32_t value) {
			return e.BeginAddress < value;
		});
		if (entry != layout.functions + layout.function_count && entry->BeginAddress == rva)
			return true;

		return (address & 0xF) == 0 && *reinterpret_cast<const std::uint8_t*>(address - 1) == 0xCC;
	}

	bool detail::image_ready()
	{
		const auto& layout = image_layout();
		return layout.functions && layout.text.begin;
	}

	detail::AddressRange detail::image_range()
	{
		return image_layout().image;
	}

	std::span<const detail::AddressRange> detail::string_sections()
	{
		return image_layout().strings;
	}

	std::vector<detail::CodeReference> detail::code_references(const std::span<const std::uintptr_t> sorted_targets)
	{
		const auto& layout = image_layout();
		std::vector<CodeReference> references;
		for (auto address = layout.text.begin; address + 7 <= layout.text.end; ++address)
		{
			const auto resolved = rip_reference(address);
			if (resolved && std::ranges::binary_search(sorted_targets, *resolved))
				references.push_back({address, *resolved});
		}
		return references;
	}

	std::optional<AnchoredFunction> detail::containing_function(const std::uintptr_t address)
	{
		const auto& layout = image_layout();
		if (address < layout.text.begin || address >= layout.text.end)
			return std::nullopt;

		const auto rva = static_cast<std::uint32_t>(address - layout.image.begin);
		const auto* entry = std::upper_bound(layout.functions, layout.functions + layout.function_count, rva, [](const std::uint32_t value, const IMAGE_RUNTIME_FUNCTION_ENTRY& e) {
			return value < e.BeginAddress;
		});
		if (entry == layout.functions)
			return std::nullopt;
		--entry;
		if (rva >= entry->EndAddress)
			return std::nullopt;

		return AnchoredFunction{reinterpret_cast<void*>(layout.image.begin + entry->BeginAddress), entry->EndAddress - entry->BeginAddress};
	}

	std::vector<AnchoredFunction> functions_calling(const void* target)
	{
		if (!detail::image_ready())
			return {};

		const auto& layout = image_layout();
		const auto wanted = reinterpret_cast<std::uintptr_t>(target);
		const auto* code = reinterpret_cast<const std::uint8_t*>(layout.text.begin);
		const std::size_t count = layout.text.end - layout.text.begin;

		std::vector<std::uintptr_t> sites;
		for (std::size_t i = 0; i + 5 <= count; ++i)
		{
			const auto site = layout.text.begin + i;
			if (is_relative_branch(code[i]) && instruction::x64_relative_target(site + 5, read_rel32(code + i + 1)) == wanted)
				sites.push_back(site);
		}

		return detail::unique_functions(sites);
	}

	std::vector<void*> calls_from(const AnchoredFunction& function)
	{
		const auto& layout = image_layout();
		const auto begin = reinterpret_cast<std::uintptr_t>(function.start);
		const auto end = begin + function.size;
		if (!layout.functions || begin < layout.text.begin || end > layout.text.end)
			return {};

		std::vector<void*> targets;
		const auto* code = reinterpret_cast<const std::uint8_t*>(begin);
		for (std::size_t i = 0; i + 5 <= function.size; ++i)
		{
			if (!is_relative_branch(code[i]))
				continue;

			const auto target = instruction::x64_relative_target(begin + i + 5, read_rel32(code + i + 1));
			if (code[i] == 0xE9 && target >= begin && target < end)
				continue;
			if (is_call_target(layout, target))
				targets.push_back(reinterpret_cast<void*>(target));
		}
		return targets;
	}

	std::vector<void*> data_references_from(const AnchoredFunction& function)
	{
		const auto& layout = image_layout();
		const auto begin = reinterpret_cast<std::uintptr_t>(function.start);
		const auto end = begin + function.size;
		if (begin < layout.text.begin || end > layout.text.end)
			return {};

		std::vector<void*> targets;
		for (auto address = begin; address + 7 <= end; ++address)
		{
			if (const auto resolved = rip_reference(address))
				targets.push_back(reinterpret_cast<void*>(*resolved));
		}
		return targets;
	}
}
