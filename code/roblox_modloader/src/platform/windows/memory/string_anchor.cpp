#include "RobloxModLoader/memory/string_anchor.hpp"

#include "RobloxModLoader/internal/common.hpp"
#include "RobloxModLoader/memory/module.hpp"
#include "RobloxModLoader/platform/memory/host_image.hpp"

#include <algorithm>
#include <cstring>
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
		std::uintptr_t rdata_begin{};
		std::uintptr_t rdata_end{};
		const IMAGE_RUNTIME_FUNCTION_ENTRY* functions{};
		std::size_t function_count{};
	};

	static ImageLayout read_image_layout()
	{
		ImageLayout layout;
		const module image(platform::studio_image_name());
		layout.base = image.begin().as<std::uintptr_t>();

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(layout.base);
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(layout.base + dos->e_lfanew);
		layout.image_end = layout.base + nt->OptionalHeader.SizeOfImage;

		const auto* section = IMAGE_FIRST_SECTION(nt);
		for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
		{
			const std::string_view name(reinterpret_cast<const char*>(section->Name),
			    strnlen(reinterpret_cast<const char*>(section->Name), IMAGE_SIZEOF_SHORT_NAME));
			if (name == ".text")
			{
				layout.text_begin = layout.base + section->VirtualAddress;
				layout.text_end = layout.text_begin + section->Misc.VirtualSize;
			}
			else if (name == ".rdata")
			{
				layout.rdata_begin = layout.base + section->VirtualAddress;
				layout.rdata_end = layout.rdata_begin + section->Misc.VirtualSize;
			}
		}

		const auto& exceptions = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
		if (exceptions.VirtualAddress)
		{
			layout.functions = reinterpret_cast<const IMAGE_RUNTIME_FUNCTION_ENTRY*>(layout.base + exceptions.VirtualAddress);
			layout.function_count = exceptions.Size / sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY);
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

	static std::optional<std::uintptr_t> rip_reference(const std::uintptr_t address)
	{
		const auto* code = reinterpret_cast<const std::uint8_t*>(address);
		if ((code[0] & 0xF8) != 0x48 || code[1] != 0x8D || (code[2] & 0xC7) != 0x05)
			return std::nullopt;

		std::int32_t displacement;
		std::memcpy(&displacement, code + 3, sizeof(displacement));
		return address + 7 + displacement;
	}

	static std::vector<std::pair<std::uintptr_t, std::uintptr_t>> code_references(const ImageLayout& layout, const std::vector<std::uintptr_t>& sorted_targets)
	{
		std::vector<std::pair<std::uintptr_t, std::uintptr_t>> references;
		for (auto address = layout.text_begin; address + 7 <= layout.text_end; ++address)
		{
			const auto resolved = rip_reference(address);
			if (resolved && std::binary_search(sorted_targets.begin(), sorted_targets.end(), *resolved))
				references.emplace_back(address, *resolved);
		}

		return references;
	}

	static std::vector<std::vector<std::uintptr_t>> string_section_addresses(const ImageLayout& layout, const std::span<const std::string_view> texts)
	{
		std::unordered_map<std::string_view, std::size_t> wanted;
		for (std::size_t i = 0; i < texts.size(); ++i)
			wanted.try_emplace(texts[i], i);

		std::vector<std::vector<std::uintptr_t>> found(texts.size());
		const auto* cursor = reinterpret_cast<const char*>(layout.rdata_begin);
		const auto* limit = reinterpret_cast<const char*>(layout.rdata_end);
		while (cursor < limit)
		{
			const auto* terminator = static_cast<const char*>(std::memchr(cursor, 0, limit - cursor));
			if (!terminator)
				break;
			if (const auto it = wanted.find(std::string_view(cursor, terminator)); it != wanted.end())
				found[it->second].push_back(reinterpret_cast<std::uintptr_t>(cursor));
			cursor = terminator + 1;
		}

		for (std::size_t i = 0; i < texts.size(); ++i)
			found[i] = found[wanted.at(texts[i])];
		return found;
	}

	static bool is_call_target(const ImageLayout& layout, const std::uintptr_t address)
	{
		if (address <= layout.text_begin || address >= layout.text_end)
			return false;

		const auto rva = static_cast<std::uint32_t>(address - layout.base);
		const auto* entry = std::lower_bound(layout.functions, layout.functions + layout.function_count, rva, [](const IMAGE_RUNTIME_FUNCTION_ENTRY& e, const std::uint32_t value) {
			return e.BeginAddress < value;
		});
		if (entry != layout.functions + layout.function_count && entry->BeginAddress == rva)
			return true;

		return (address & 0xF) == 0 && *reinterpret_cast<const std::uint8_t*>(address - 1) == 0xCC;
	}

	static std::optional<AnchoredFunction> containing_function(const ImageLayout& layout, const std::uintptr_t address)
	{
		if (address < layout.text_begin || address >= layout.text_end)
			return std::nullopt;

		const auto rva = static_cast<std::uint32_t>(address - layout.base);
		const auto* entry = std::upper_bound(layout.functions, layout.functions + layout.function_count, rva, [](const std::uint32_t value, const IMAGE_RUNTIME_FUNCTION_ENTRY& e) {
			return value < e.BeginAddress;
		});
		if (entry == layout.functions)
			return std::nullopt;
		--entry;
		if (rva >= entry->EndAddress)
			return std::nullopt;

		return AnchoredFunction{reinterpret_cast<void*>(layout.base + entry->BeginAddress), entry->EndAddress - entry->BeginAddress};
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
		if (!layout.functions || !layout.text_begin)
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
		if (!layout.functions || !layout.text_begin || !layout.rdata_begin)
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
		if (!layout.functions || !layout.text_begin)
			return {};

		const auto wanted = reinterpret_cast<std::uintptr_t>(target);
		const auto* code = reinterpret_cast<const std::uint8_t*>(layout.text_begin);
		const std::size_t count = layout.text_end - layout.text_begin;

		std::vector<std::uintptr_t> sites;
		for (std::size_t i = 0; i + 5 <= count; ++i)
		{
			if (code[i] != 0xE8 && code[i] != 0xE9)
				continue;

			std::int32_t displacement;
			std::memcpy(&displacement, code + i + 1, sizeof(displacement));
			if (layout.text_begin + i + 5 + displacement == wanted)
				sites.push_back(layout.text_begin + i);
		}

		return unique_functions(layout, sites);
	}

	std::vector<void*> calls_from(const AnchoredFunction& function)
	{
		const auto& layout = image_layout();
		const auto begin = reinterpret_cast<std::uintptr_t>(function.start);
		const auto end = begin + function.size;
		if (!layout.functions || begin < layout.text_begin || end > layout.text_end)
			return {};

		std::vector<void*> targets;
		const auto* code = reinterpret_cast<const std::uint8_t*>(begin);
		for (std::size_t i = 0; i + 5 <= function.size; ++i)
		{
			if (code[i] != 0xE8 && code[i] != 0xE9)
				continue;

			std::int32_t displacement;
			std::memcpy(&displacement, code + i + 1, sizeof(displacement));
			const auto target = begin + i + 5 + displacement;
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
		if (begin < layout.text_begin || end > layout.text_end)
			return {};

		std::vector<void*> targets;
		for (auto address = begin; address + 7 <= end; ++address)
		{
			if (const auto resolved = rip_reference(address))
				targets.push_back(reinterpret_cast<void*>(*resolved));
		}
		return targets;
	}

	std::optional<AnchoredFunction> function_containing(const void* address)
	{
		const auto& layout = image_layout();
		if (!layout.functions || !layout.text_begin)
			return std::nullopt;
		return containing_function(layout, reinterpret_cast<std::uintptr_t>(address));
	}
}
