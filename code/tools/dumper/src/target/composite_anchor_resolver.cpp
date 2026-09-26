#include "target/composite_anchor_resolver.hpp"

#include "target/call_path.hpp"
#include "target/pattern_anchor_resolver.hpp"
#include "target/text_anchor_resolver.hpp"

#include <string>
#include <vector>

namespace rml::dumper::target
{
	std::expected<AnchorSet, Error> CompositeAnchorResolver::resolve(const image::Image& image,
	                                                                 const std::span<const AnchorSpec> anchors) const
	{
		std::vector<AnchorSpec> by_text;
		std::vector<AnchorSpec> by_pattern;
		std::vector<AnchorSpec> by_origin;
		std::string undeclared;

		for (const auto& anchor : anchors)
		{
			if (anchor.origin)
				by_origin.push_back(anchor);
			else if (!anchor.text.empty())
				by_text.push_back(anchor);
			else if (!anchor.pattern.empty())
				by_pattern.push_back(anchor);
			else
				undeclared += std::format("\n  {}: no pattern and no literal", to_string(anchor.id));
		}

		if (!undeclared.empty())
			return std::unexpected(
			    Error::make(ErrorCode::anchor, "anchors with nothing to locate them by:{}", undeclared));

		AnchorSet resolved;
		std::string failures;

		const auto merge = [&resolved, &failures](const std::expected<AnchorSet, Error>& outcome,
		                                          const std::span<const AnchorSpec> subset) {
			if (!outcome)
			{
				failures += "\n" + outcome.error().message();
				return;
			}

			for (const auto& anchor : subset)
				if (outcome->has(anchor.id))
					resolved.set(anchor.id, outcome->at(anchor.id));
		};

		if (!by_text.empty())
			merge(TextAnchorResolver().resolve(image, by_text), by_text);

		if (!by_pattern.empty())
			merge(PatternAnchorResolver().resolve(image, by_pattern), by_pattern);

		for (const auto& anchor : by_origin)
		{
			if (!resolved.has(*anchor.origin))
			{
				failures += std::format("\n  {}: {} is not resolved", to_string(anchor.id), to_string(*anchor.origin));
				continue;
			}

			const auto target = follow_path(image, resolved.at(*anchor.origin), anchor.path);
			if (!target)
			{
				failures += std::format("\n  {}: from {}: {}", to_string(anchor.id), to_string(*anchor.origin), target.error());
				continue;
			}

			resolved.set(anchor.id, *target);
		}

		if (!failures.empty())
			return std::unexpected(Error::make(ErrorCode::anchor, "{}", failures));

		return resolved;
	}
}
