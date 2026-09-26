#include "string_anchor_image.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>

namespace rml::memory
{
	static std::vector<std::uintptr_t> exact_string_addresses(const detail::AddressRange image, const std::string_view text)
	{
		const std::string_view haystack{reinterpret_cast<const char*>(image.begin), image.end - image.begin};
		std::string needle;
		needle.push_back('\0');
		needle.append(text);
		needle.push_back('\0');

		std::vector<std::uintptr_t> found;
		for (auto position = haystack.find(needle); position != std::string_view::npos; position = haystack.find(needle, position + 1))
			found.push_back(image.begin + position + 1);
		return found;
	}

	static std::vector<std::vector<std::uintptr_t>> string_section_addresses(const std::span<const std::string_view> texts)
	{
		std::unordered_map<std::string_view, std::size_t> wanted;
		for (std::size_t i = 0; i < texts.size(); ++i)
			wanted.try_emplace(texts[i], i);

		std::vector<std::vector<std::uintptr_t>> found(texts.size());
		for (const auto [begin, end] : detail::string_sections())
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

	std::vector<AnchoredFunction> detail::unique_functions(const std::span<const std::uintptr_t> addresses)
	{
		std::vector<AnchoredFunction> result;
		for (const auto address : addresses)
		{
			const auto function = containing_function(address);
			if (function && std::ranges::none_of(result, [&](const AnchoredFunction& f) { return f.start == function->start; }))
				result.push_back(*function);
		}
		return result;
	}

	std::vector<AnchoredFunction> functions_referencing_string(const std::string_view exact_text)
	{
		if (!detail::image_ready())
			return {};

		auto strings = exact_string_addresses(detail::image_range(), exact_text);
		if (strings.empty())
			return {};

		std::ranges::sort(strings);
		std::vector<std::uintptr_t> sites;
		for (const auto& reference : detail::code_references(strings))
			sites.push_back(reference.site);
		return detail::unique_functions(sites);
	}

	std::vector<std::vector<AnchoredFunction>> functions_referencing_strings(const std::span<const std::string_view> exact_texts)
	{
		std::vector<std::vector<AnchoredFunction>> result(exact_texts.size());
		if (!detail::image_ready())
			return result;

		const auto addresses = string_section_addresses(exact_texts);
		std::vector<std::pair<std::uintptr_t, std::size_t>> owners;
		for (std::size_t i = 0; i < addresses.size(); ++i)
			for (const auto address : addresses[i])
				owners.emplace_back(address, i);
		std::ranges::sort(owners);

		std::vector<std::uintptr_t> targets;
		targets.reserve(owners.size());
		for (const auto& [address, index] : owners)
			targets.push_back(address);

		std::vector<std::vector<std::uintptr_t>> sites(exact_texts.size());
		for (const auto& reference : detail::code_references(targets))
		{
			const auto [first, last] = std::ranges::equal_range(owners, reference.target, {}, &std::pair<std::uintptr_t, std::size_t>::first);
			for (auto it = first; it != last; ++it)
				sites[it->second].push_back(reference.site);
		}

		for (std::size_t i = 0; i < exact_texts.size(); ++i)
			result[i] = detail::unique_functions(sites[i]);
		return result;
	}

	std::optional<AnchoredFunction> function_containing(const void* address)
	{
		if (!detail::image_ready())
			return std::nullopt;
		return detail::containing_function(reinterpret_cast<std::uintptr_t>(address));
	}
}
