#include "RobloxModLoader/platform/memory/import_slots.hpp"

#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/memory_protection.hpp"

#include <atomic>
#include <cstring>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

namespace rml::platform
{
	static bool write_slot(void** slot, void* value) noexcept
	{
		const auto previous = set_protection(slot, sizeof(void*), utils::MemoryProtection::ReadWrite);
		if (!previous)
			return false;
		std::atomic_ref(*slot).store(value, std::memory_order_release);
		restore_protection(slot, sizeof(void*), *previous);
		return true;
	}

	static bool skipped(const std::uint32_t index)
	{
		return index == INDIRECT_SYMBOL_ABS || index == INDIRECT_SYMBOL_LOCAL || index == (INDIRECT_SYMBOL_ABS | INDIRECT_SYMBOL_LOCAL);
	}

	std::vector<ImportSlot> find_import_slots(const std::string_view image, const std::string_view symbol)
	{
		std::vector<ImportSlot> found;
		const memory::module host(image);
		const auto base = host.begin().as<std::uintptr_t>();
		if (!base)
			return found;

		const auto* header = reinterpret_cast<const mach_header_64*>(base);
		const segment_command_64* linkedit = nullptr;
		const symtab_command* symtab = nullptr;
		const dysymtab_command* dysymtab = nullptr;
		std::uintptr_t slide = 0;
		std::vector<const section_64*> sections;

		const auto* command = reinterpret_cast<const load_command*>(header + 1);
		for (std::uint32_t index = 0; index < header->ncmds; ++index)
		{
			if (command->cmd == LC_SEGMENT_64)
			{
				const auto* segment = reinterpret_cast<const segment_command_64*>(command);
				if (std::strncmp(segment->segname, SEG_TEXT, sizeof(segment->segname)) == 0)
					slide = base - segment->vmaddr;
				else if (std::strncmp(segment->segname, SEG_LINKEDIT, sizeof(segment->segname)) == 0)
					linkedit = segment;
				const auto* section = reinterpret_cast<const section_64*>(segment + 1);
				for (std::uint32_t at = 0; at < segment->nsects; ++at, ++section)
				{
					const auto type = section->flags & SECTION_TYPE;
					if (type == S_LAZY_SYMBOL_POINTERS || type == S_NON_LAZY_SYMBOL_POINTERS)
						sections.push_back(section);
				}
			}
			else if (command->cmd == LC_SYMTAB)
				symtab = reinterpret_cast<const symtab_command*>(command);
			else if (command->cmd == LC_DYSYMTAB)
				dysymtab = reinterpret_cast<const dysymtab_command*>(command);
			command = reinterpret_cast<const load_command*>(reinterpret_cast<const std::uint8_t*>(command) + command->cmdsize);
		}
		if (!linkedit || !symtab || !dysymtab)
			return found;

		const auto linkedit_base = slide + linkedit->vmaddr - linkedit->fileoff;
		const auto* symbols = reinterpret_cast<const nlist_64*>(linkedit_base + symtab->symoff);
		const auto* strings = reinterpret_cast<const char*>(linkedit_base + symtab->stroff);
		const auto* indirect = reinterpret_cast<const std::uint32_t*>(linkedit_base + dysymtab->indirectsymoff);

		for (const auto* section : sections)
		{
			auto** slots = reinterpret_cast<void**>(slide + section->addr);
			const auto count = section->size / sizeof(void*);
			for (std::size_t at = 0; at < count; ++at)
			{
				const auto symbol_index = indirect[section->reserved1 + at];
				if (skipped(symbol_index))
					continue;
				const std::string_view name = strings + symbols[symbol_index].n_un.n_strx;
				if (name.size() == symbol.size() + 1 && name.front() == '_' && name.substr(1) == symbol)
					found.push_back({&slots[at], slots[at]});
			}
		}
		return found;
	}

	bool rebind_import_slots(const std::span<const ImportSlot> slots, void* replacement) noexcept
	{
		bool written = !slots.empty();
		for (const auto& slot : slots)
			written = write_slot(slot.slot, replacement) && written;
		return written;
	}

	void restore_import_slots(const std::span<const ImportSlot> slots) noexcept
	{
		for (const auto& slot : slots)
			write_slot(slot.slot, slot.bound);
	}
}
