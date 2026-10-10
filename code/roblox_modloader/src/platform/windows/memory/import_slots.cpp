#include "RobloxModLoader/platform/memory/import_slots.hpp"

#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/memory_protection.hpp"

#include <atomic>
#include <cstring>

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

	std::vector<ImportSlot> find_import_slots(const std::string_view image, const std::string_view symbol)
	{
		std::vector<ImportSlot> found;
		const memory::module host(image);
		const auto base = host.begin().as<std::uintptr_t>();
		if (!base)
			return found;

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
		const auto& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		if (!directory.VirtualAddress)
			return found;

		for (auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + directory.VirtualAddress);
		    descriptor->Name;
		    ++descriptor)
		{
			if (!descriptor->OriginalFirstThunk)
				continue;
			const auto* names = reinterpret_cast<const IMAGE_THUNK_DATA64*>(base + descriptor->OriginalFirstThunk);
			auto* slots = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + descriptor->FirstThunk);
			for (; names->u1.AddressOfData; ++names, ++slots)
			{
				if (IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))
					continue;
				const auto* by_name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
				if (std::string_view(reinterpret_cast<const char*>(by_name->Name)) == symbol)
				{
					auto** slot = reinterpret_cast<void**>(&slots->u1.Function);
					found.push_back({slot, *slot});
				}
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
