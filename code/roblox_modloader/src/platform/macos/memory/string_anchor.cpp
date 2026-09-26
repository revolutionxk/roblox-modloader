#include "memory/string_anchor_image.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/instruction.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

#include <algorithm>
#include <cstring>
#include <mach-o/loader.h>
#include <string_view>

namespace rml::memory
{
	struct ImageLayout
	{
		std::uintptr_t base{};
		std::uintptr_t text_begin{};
		std::uintptr_t text_end{};
		std::uintptr_t image_end{};
		std::vector<std::uintptr_t> function_starts;
		std::vector<detail::AddressRange> string_sections;
	};

	static ImageLayout read_image_layout()
	{
		ImageLayout layout;
		const module image(platform::studio_image_name());
		layout.base = image.begin().as<std::uintptr_t>();
		layout.image_end = layout.base + image.size();

		const auto* header = reinterpret_cast<const mach_header_64*>(layout.base);
		const auto* command = reinterpret_cast<const load_command*>(header + 1);
		std::uintptr_t slide = 0;
		std::uintptr_t linkedit_vmaddr = 0;
		std::uintptr_t linkedit_fileoff = 0;
		std::uintptr_t text_vmaddr = 0;
		std::uint32_t starts_offset = 0;
		std::uint32_t starts_size = 0;

		for (std::uint32_t i = 0; i < header->ncmds; ++i)
		{
			if (command->cmd == LC_SEGMENT_64)
			{
				const auto* segment = reinterpret_cast<const segment_command_64*>(command);
				const std::string_view name(segment->segname);
				const auto* section = reinterpret_cast<const section_64*>(segment + 1);
				for (std::uint32_t s = 0; s < segment->nsects; ++s, ++section)
				{
					if (name == "__TEXT" && std::string_view(section->sectname) == "__text")
					{
						layout.text_begin = section->addr;
						layout.text_end = section->addr + section->size;
					}
					if ((section->flags & SECTION_TYPE) == S_CSTRING_LITERALS)
						layout.string_sections.push_back({section->addr, section->addr + section->size});
				}

				if (name == "__TEXT")
				{
					text_vmaddr = segment->vmaddr;
					slide = layout.base - segment->vmaddr;
				}
				else if (name == "__LINKEDIT")
				{
					linkedit_vmaddr = segment->vmaddr;
					linkedit_fileoff = segment->fileoff;
				}
			}
			else if (command->cmd == LC_FUNCTION_STARTS)
			{
				const auto* entry = reinterpret_cast<const linkedit_data_command*>(command);
				starts_offset = entry->dataoff;
				starts_size = entry->datasize;
			}
			command = reinterpret_cast<const load_command*>(reinterpret_cast<const std::byte*>(command) + command->cmdsize);
		}

		if (layout.text_begin)
		{
			layout.text_begin += slide;
			layout.text_end += slide;
		}
		for (auto& [begin, end] : layout.string_sections)
		{
			begin += slide;
			end += slide;
		}

		if (starts_size && linkedit_vmaddr)
		{
			const auto* cursor = reinterpret_cast<const std::uint8_t*>(linkedit_vmaddr + slide + (starts_offset - linkedit_fileoff));
			const auto* end = cursor + starts_size;
			std::uintptr_t address = text_vmaddr + slide;
			while (cursor < end)
			{
				std::uint64_t delta = 0;
				unsigned shift = 0;
				std::uint8_t byte;
				do
				{
					byte = *cursor++;
					delta |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
					shift += 7;
				} while ((byte & 0x80) && cursor < end);
				if (delta == 0 && !layout.function_starts.empty())
					break;
				address += delta;
				layout.function_starts.push_back(address);
			}
			std::sort(layout.function_starts.begin(), layout.function_starts.end());
		}

		return layout;
	}

	static const ImageLayout& image_layout()
	{
		static const ImageLayout layout = read_image_layout();
		return layout;
	}

	static std::optional<std::uintptr_t> page_reference(const std::uintptr_t pc, const std::uint32_t adrp, const std::uint32_t next)
	{
		if (!instruction::arm64_is_adrp(adrp) || instruction::arm64_rn(next) != instruction::arm64_rd(adrp))
			return std::nullopt;

		const auto offset = instruction::arm64_page_offset(next);
		if (!offset)
			return std::nullopt;
		return instruction::arm64_adrp_page(pc, adrp) + *offset;
	}

	bool detail::image_ready()
	{
		const auto& layout = image_layout();
		return !layout.function_starts.empty() && layout.text_begin;
	}

	detail::AddressRange detail::image_range()
	{
		const auto& layout = image_layout();
		return {layout.base, layout.image_end};
	}

	std::span<const detail::AddressRange> detail::string_sections()
	{
		return image_layout().string_sections;
	}

	std::vector<detail::CodeReference> detail::code_references(const std::span<const std::uintptr_t> sorted_targets)
	{
		const auto& layout = image_layout();
		const auto* code = reinterpret_cast<const std::uint32_t*>(layout.text_begin);
		const std::size_t count = (layout.text_end - layout.text_begin) / 4;

		std::vector<CodeReference> references;
		for (std::size_t i = 0; i + 1 < count; ++i)
		{
			const auto pc = layout.text_begin + i * 4;
			const auto resolved = page_reference(pc, code[i], code[i + 1]);
			if (resolved && std::ranges::binary_search(sorted_targets, *resolved))
				references.push_back({pc, *resolved});
		}
		return references;
	}

	std::optional<AnchoredFunction> detail::containing_function(const std::uintptr_t address)
	{
		const auto& layout = image_layout();
		if (address < layout.text_begin || address >= layout.text_end)
			return std::nullopt;

		const auto it = std::upper_bound(layout.function_starts.begin(), layout.function_starts.end(), address);
		if (it == layout.function_starts.begin())
			return std::nullopt;

		const auto start = *(it - 1);
		const auto end = it == layout.function_starts.end() ? layout.text_end : *it;
		return AnchoredFunction{reinterpret_cast<void*>(start), end - start};
	}

	std::vector<AnchoredFunction> functions_calling(const void* target)
	{
		if (!detail::image_ready())
			return {};

		const auto& layout = image_layout();
		const auto wanted = reinterpret_cast<std::uintptr_t>(target);
		const auto* code = reinterpret_cast<const std::uint32_t*>(layout.text_begin);
		const std::size_t count = (layout.text_end - layout.text_begin) / 4;

		std::vector<std::uintptr_t> sites;
		for (std::size_t i = 0; i < count; ++i)
		{
			const auto pc = layout.text_begin + i * 4;
			if (instruction::arm64_is_branch(code[i]) && instruction::arm64_branch_target(pc, code[i]) == wanted)
				sites.push_back(pc);
		}

		return detail::unique_functions(sites);
	}

	std::vector<void*> calls_from(const AnchoredFunction& function)
	{
		const auto& layout = image_layout();
		const auto begin = reinterpret_cast<std::uintptr_t>(function.start);
		const auto end = begin + function.size;
		if (layout.function_starts.empty() || begin < layout.text_begin || end > layout.text_end)
			return {};

		std::vector<void*> targets;
		const auto* code = reinterpret_cast<const std::uint32_t*>(begin);
		for (std::size_t i = 0; i < function.size / 4; ++i)
		{
			const auto word = code[i];
			if (!instruction::arm64_is_branch(word))
				continue;

			const auto target = instruction::arm64_branch_target(begin + i * 4, word);
			if (!instruction::arm64_is_linked_branch(word) && target >= begin && target < end)
				continue;
			if (std::binary_search(layout.function_starts.begin(), layout.function_starts.end(), target))
				targets.push_back(reinterpret_cast<void*>(target));
		}
		return targets;
	}

	std::vector<void*> data_references_from(const AnchoredFunction& function)
	{
		const auto& layout = image_layout();
		const auto begin = reinterpret_cast<std::uintptr_t>(function.start);
		if (begin < layout.text_begin || begin + function.size > layout.text_end)
			return {};

		std::vector<void*> targets;
		const auto* code = reinterpret_cast<const std::uint32_t*>(begin);
		for (std::size_t i = 0; i + 1 < function.size / 4; ++i)
		{
			if (const auto resolved = page_reference(begin + i * 4, code[i], code[i + 1]))
				targets.push_back(reinterpret_cast<void*>(*resolved));
		}
		return targets;
	}
}
