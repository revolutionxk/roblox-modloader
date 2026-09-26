#include "target/text_anchor_resolver.hpp"

#include "target/call_path.hpp"

#include <algorithm>
#include <cstring>
#include <format>
#include <set>
#include <string>

namespace rml::dumper::target
{
	static constexpr std::size_t rip_lea_length = 7;

	static bool is_rip_relative_lea(const std::span<const std::byte> code, const std::size_t offset)
	{
		if (offset + rip_lea_length > code.size())
			return false;

		const auto rex = static_cast<std::uint8_t>(code[offset]);
		if (rex < 0x48 || rex > 0x4F)
			return false;

		if (static_cast<std::uint8_t>(code[offset + 1]) != 0x8D)
			return false;

		return (static_cast<std::uint8_t>(code[offset + 2]) & 0xC7) == 0x05;
	}

	std::vector<Rva> TextAnchorResolver::find_literal(const image::Image& image, const std::string_view text)
	{
		std::vector<Rva> found;
		if (text.empty())
			return found;

		for (const auto& section : image.sections())
		{
			if (section.executable)
				continue;

			const auto bytes = image.at(section.address, section.size);
			if (bytes.size() < text.size() + 1)
				continue;

			const auto* begin = reinterpret_cast<const char*>(bytes.data());
			const auto* end = begin + bytes.size();

			for (const char* cursor = begin;; ++cursor)
			{
				cursor = std::search(cursor, end, text.begin(), text.end());
				if (cursor == end)
					break;

				const auto position = static_cast<std::size_t>(cursor - begin);
				if (position + text.size() >= bytes.size())
					break;

				const bool terminated = cursor[text.size()] == '\0';
				const bool standalone = position == 0 || cursor[-1] == '\0';

				if (terminated && standalone)
					found.push_back(section.address + static_cast<Rva>(position));
			}
		}

		return found;
	}

	std::vector<Rva> TextAnchorResolver::find_rip_references(const image::Image& image, const Rva target)
	{
		std::vector<Rva> references;

		for (const auto& section : image.executable_sections())
		{
			const auto code = image.at(section.address, section.size);
			if (code.empty())
				continue;

			for (std::size_t offset = 0; offset + rip_lea_length <= code.size(); ++offset)
			{
				if (!is_rip_relative_lea(code, offset))
					continue;

				std::int32_t displacement = 0;
				std::memcpy(&displacement, code.data() + offset + 3, sizeof(displacement));

				const auto next = section.address + static_cast<Rva>(offset + rip_lea_length);
				if (static_cast<Rva>(static_cast<std::int64_t>(next) + displacement) == target)
					references.push_back(section.address + static_cast<Rva>(offset));
			}
		}

		return references;
	}

	std::optional<Rva> TextAnchorResolver::owning_function(const image::Image& image, const Rva address)
	{
		const auto bounds = image.functions().containing(address);
		if (!bounds)
			return std::nullopt;

		return bounds->begin;
	}

	std::expected<AnchorSet, Error> TextAnchorResolver::resolve(const image::Image& image,
	                                                            const std::span<const AnchorSpec> anchors) const
	{
		AnchorSet resolved;
		std::string failures;

		for (const auto& anchor : anchors)
		{
			const auto name = to_string(anchor.id);

			if (anchor.text.empty())
			{
				failures += std::format("\n  {}: no literal to search for", name);
				continue;
			}

			const auto literals = find_literal(image, anchor.text);
			if (literals.empty())
			{
				failures += std::format("\n  {}: the literal \"{}\" is not in the image", name, anchor.text);
				continue;
			}

			std::set<Rva> owners;
			for (const auto literal : literals)
				for (const auto reference : find_rip_references(image, literal))
					if (const auto owner = owning_function(image, reference))
						owners.insert(*owner);

			if (owners.empty())
			{
				failures += std::format("\n  {}: nothing references the literal \"{}\"", name, anchor.text);
				continue;
			}

			if (owners.size() > 1)
			{
				failures += std::format("\n  {}: the literal \"{}\" is referenced from {} functions at", name,
				                        anchor.text, owners.size());
				std::size_t shown = 0;
				for (const auto owner : owners)
				{
					if (shown++ == 4)
						break;

					failures += std::format(" 0x{:X}", owner);
				}
				continue;
			}

			const auto target = follow_path(image, *owners.begin(), anchor.path);
			if (!target)
			{
				failures += std::format("\n  {}: from the literal \"{}\": {}", name, anchor.text, target.error());
				continue;
			}

			resolved.set(anchor.id, *target);
		}

		if (!failures.empty())
			return std::unexpected(Error::make(ErrorCode::anchor, "text anchors did not resolve uniquely:{}",
			                                   failures));

		return resolved;
	}
}
