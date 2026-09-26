#include "RobloxModLoader/memory/string_anchor.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

#include <algorithm>
#include <cstring>
#include <mach-o/loader.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace rml::memory
{
	struct ImageLayout
	{
		std::uintptr_t base{};
		std::uintptr_t text_begin{};
		std::uintptr_t text_end{};
		std::uintptr_t image_end{};
		std::vector<std::uintptr_t> function_starts;
		std::vector<std::pair<std::uintptr_t, std::uintptr_t>> string_sections;
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
						layout.string_sections.emplace_back(section->addr, section->addr + section->size);
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

	static std::vector<std::uintptr_t> exact_string_addresses(const ImageLayout& layout, const std::string_view text)
	{
		const std::string_view haystack{reinterpret_cast<const char*>(layout.base), layout.image_end - layout.base};
		std::string needle;
		needle.push_back('\0');
		needle.append(text);
		needle.push_back('\0');

		std::vector<std::uintptr_t> found;
		for (auto position = haystack.find(needle); position != std::string_view::npos; position = haystack.find(needle, position + 1))
			found.push_back(layout.base + position + 1);
		return found;
	}

	static std::int64_t adrp_page(const std::uintptr_t pc, const std::uint32_t instruction)
	{
		const std::int64_t immlo = (instruction >> 29) & 0x3;
		const std::int64_t immhi = (instruction >> 5) & 0x7FFFF;
		std::int64_t imm = ((immhi << 2) | immlo) << 12;
		if (imm & (std::int64_t{1} << 32))
			imm -= std::int64_t{1} << 33;
		return static_cast<std::int64_t>(pc & ~std::uintptr_t{0xFFF}) + imm;
	}

	static std::optional<std::uintptr_t> page_reference(const std::uintptr_t pc, const std::uint32_t instruction, const std::uint32_t next)
	{
		if ((instruction & 0x9F000000) != 0x90000000)
			return std::nullopt;

		const auto page = adrp_page(pc, instruction);
		const auto rd = instruction & 0x1F;
		const auto rn = (next >> 5) & 0x1F;
		if (rn != rd)
			return std::nullopt;

		if ((next & 0xFF800000) == 0x91000000)
			return page + ((next >> 10) & 0xFFF);
		if ((next & 0x3B000000) == 0x39000000)
			return page + (static_cast<std::int64_t>((next >> 10) & 0xFFF) << (next >> 30));
		return std::nullopt;
	}

	static std::vector<std::pair<std::uintptr_t, std::uintptr_t>> code_references(const ImageLayout& layout, const std::vector<std::uintptr_t>& sorted_targets)
	{
		std::vector<std::pair<std::uintptr_t, std::uintptr_t>> references;
		const auto* code = reinterpret_cast<const std::uint32_t*>(layout.text_begin);
		const std::size_t count = (layout.text_end - layout.text_begin) / 4;

		for (std::size_t i = 0; i + 1 < count; ++i)
		{
			const auto pc = layout.text_begin + i * 4;
			const auto resolved = page_reference(pc, code[i], code[i + 1]);
			if (resolved && std::binary_search(sorted_targets.begin(), sorted_targets.end(), *resolved))
				references.emplace_back(pc, *resolved);
		}

		return references;
	}

	static std::vector<std::vector<std::uintptr_t>> string_section_addresses(const ImageLayout& layout, const std::span<const std::string_view> texts)
	{
		std::unordered_map<std::string_view, std::size_t> wanted;
		for (std::size_t i = 0; i < texts.size(); ++i)
			wanted.try_emplace(texts[i], i);

		std::vector<std::vector<std::uintptr_t>> found(texts.size());
		for (const auto [begin, end] : layout.string_sections)
		{
			const auto* cursor = reinterpret_cast<const char*>(begin);
			const auto* limit = reinterpret_cast<const char*>(end);
			while (cursor < limit)
			{
				const auto* terminator = static_cast<const char*>(std::memchr(cursor, 0, limit - cursor));
				if (!terminator)
					break;
				if (const auto it = wanted.find(std::string_view(cursor, terminator)); it != wanted.end())
					found[it->second].push_back(reinterpret_cast<std::uintptr_t>(cursor));
				cursor = terminator + 1;
			}
		}

		for (std::size_t i = 0; i < texts.size(); ++i)
			found[i] = found[wanted.at(texts[i])];
		return found;
	}

	static std::uintptr_t branch_target(const std::uintptr_t pc, const std::uint32_t instruction)
	{
		std::int64_t imm = instruction & 0x03FFFFFF;
		if (imm & 0x02000000)
			imm -= 0x04000000;
		return pc + imm * 4;
	}

	static std::optional<AnchoredFunction> containing_function(const ImageLayout& layout, const std::uintptr_t address)
	{
		if (address < layout.text_begin || address >= layout.text_end)
			return std::nullopt;

		const auto it = std::upper_bound(layout.function_starts.begin(), layout.function_starts.end(), address);
		if (it == layout.function_starts.begin())
			return std::nullopt;

		const auto start = *(it - 1);
		const auto end = it == layout.function_starts.end() ? layout.text_end : *it;
		return AnchoredFunction{reinterpret_cast<void*>(start), end - start};
	}

	static std::vector<AnchoredFunction> unique_functions(const ImageLayout& layout, const std::vector<std::uintptr_t>& addresses)
	{
		std::vector<AnchoredFunction> result;
		for (const auto address : addresses)
		{
			const auto function = containing_function(layout, address);
			if (!function)
				continue;
			if (std::none_of(result.begin(), result.end(), [&](const AnchoredFunction& f) {
				    return f.start == function->start;
			    }))
				result.push_back(*function);
		}
		return result;
	}

	std::vector<AnchoredFunction> functions_referencing_string(const std::string_view exact_text)
	{
		const auto& layout = image_layout();
		if (layout.function_starts.empty() || !layout.text_begin)
			return {};

		auto strings = exact_string_addresses(layout, exact_text);
		if (strings.empty())
			return {};

		std::ranges::sort(strings);
		std::vector<std::uintptr_t> sites;
		for (const auto [site, target] : code_references(layout, strings))
			sites.push_back(site);
		return unique_functions(layout, sites);
	}

	std::vector<std::vector<AnchoredFunction>> functions_referencing_strings(const std::span<const std::string_view> exact_texts)
	{
		std::vector<std::vector<AnchoredFunction>> result(exact_texts.size());
		const auto& layout = image_layout();
		if (layout.function_starts.empty() || !layout.text_begin)
			return result;

		const auto addresses = string_section_addresses(layout, exact_texts);
		std::vector<std::pair<std::uintptr_t, std::size_t>> owners;
		for (std::size_t i = 0; i < addresses.size(); ++i)
			for (const auto address : addresses[i])
				owners.emplace_back(address, i);
		std::ranges::sort(owners);

		std::vector<std::uintptr_t> targets;
		for (const auto& [address, index] : owners)
			targets.push_back(address);

		std::vector<std::vector<std::uintptr_t>> sites(exact_texts.size());
		for (const auto [site, target] : code_references(layout, targets))
		{
			const auto [first, last] = std::ranges::equal_range(owners, target, {}, &std::pair<std::uintptr_t, std::size_t>::first);
			for (auto it = first; it != last; ++it)
				sites[it->second].push_back(site);
		}

		for (std::size_t i = 0; i < exact_texts.size(); ++i)
			result[i] = unique_functions(layout, sites[i]);
		return result;
	}

	std::vector<AnchoredFunction> functions_calling(const void* target)
	{
		const auto& layout = image_layout();
		if (layout.function_starts.empty() || !layout.text_begin)
			return {};

		const auto wanted = reinterpret_cast<std::uintptr_t>(target);
		const auto* code = reinterpret_cast<const std::uint32_t*>(layout.text_begin);
		const std::size_t count = (layout.text_end - layout.text_begin) / 4;

		std::vector<std::uintptr_t> sites;
		for (std::size_t i = 0; i < count; ++i)
		{
			const auto instruction = code[i];
			if ((instruction & 0x7C000000) != 0x14000000)
				continue;

			const auto pc = layout.text_begin + i * 4;
			if (branch_target(pc, instruction) == wanted)
				sites.push_back(pc);
		}

		return unique_functions(layout, sites);
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
			const auto instruction = code[i];
			if ((instruction & 0x7C000000) != 0x14000000)
				continue;

			const auto target = branch_target(begin + i * 4, instruction);
			const bool linked = (instruction & 0x80000000) != 0;
			if (!linked && target >= begin && target < end)
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

	std::optional<AnchoredFunction> function_containing(const void* address)
	{
		const auto& layout = image_layout();
		if (layout.function_starts.empty() || !layout.text_begin)
			return std::nullopt;
		return containing_function(layout, reinterpret_cast<std::uintptr_t>(address));
	}
}
