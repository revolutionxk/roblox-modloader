#include <doctest/doctest.h>

#include "rml/dumper/scan/scanner.hpp"
#include "rml/dumper/target/target_profile.hpp"
#include "support/studio_binary.hpp"

#include <algorithm>
#include <chrono>

using namespace rml::dumper;
using namespace rml::dumper::target;

static const std::array<Anchor, 0> unresolved_on_this_build{};

TEST_CASE("the windows anchors are scanned for in one fast pass")
{
	const auto path = tests::StudioBinary::windows();
	if (!path)
	{
		MESSAGE("RML_TEST_STUDIO is not set, skipping");
		return;
	}

	const auto* profile = TargetRegistry::find("windows-x64");
	REQUIRE(profile != nullptr);

	const auto image = image::ImageLoader::load(*path, profile->architecture);
	REQUIRE(image.has_value());

	std::vector<AnchorSpec> scanned;
	std::vector<scan::Pattern> patterns;
	for (const auto& spec : profile->anchors)
	{
		if (spec.pattern.empty())
			continue;
		scanned.push_back(spec);
		patterns.push_back(*scan::Pattern::parse(spec.pattern));
	}

	const auto started = std::chrono::steady_clock::now();
	const auto hits = scan::Scanner().scan(*image, patterns);
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
	    std::chrono::steady_clock::now() - started);

	MESSAGE("scanned ", image->executable_sections()[0].size, " bytes of code for ", patterns.size(),
	        " patterns in ", elapsed.count(), " ms");

	std::size_t unique = 0;
	std::size_t missing = 0;
	std::size_t ambiguous = 0;

	for (std::size_t i = 0; i < scanned.size(); ++i)
	{
		const auto name = to_string(scanned[i].id);

		if (hits[i].empty())
		{
			++missing;
			MESSAGE(name, ": no match");
		}
		else if (hits[i].size() > 1)
		{
			++ambiguous;
			MESSAGE(name, ": ", hits[i].size(), " matches, first at 0x", hits[i].front());
		}
		else
		{
			++unique;
			CHECK_MESSAGE(image->functions().at(hits[i].front()).has_value(), name, " does not start a function");
		}
	}

	MESSAGE(unique, " resolved uniquely, ", missing, " unmatched, ", ambiguous, " ambiguous");

	CHECK(unique == scanned.size() - unresolved_on_this_build.size());
}

TEST_CASE("the anchors known to still match keep matching")
{
	const auto path = tests::StudioBinary::windows();
	if (!path)
	{
		MESSAGE("RML_TEST_STUDIO is not set, skipping");
		return;
	}

	const auto* profile = TargetRegistry::find("windows-x64");
	const auto image = image::ImageLoader::load(*path, profile->architecture);
	REQUIRE(image.has_value());

	std::vector<AnchorSpec> healthy;
	for (const auto& spec : profile->anchors)
		if (!spec.pattern.empty() && std::ranges::find(unresolved_on_this_build, spec.id) == unresolved_on_this_build.end())
			healthy.push_back(spec);

	std::vector<scan::Pattern> patterns;
	for (const auto& spec : healthy)
		patterns.push_back(*scan::Pattern::parse(spec.pattern));

	const auto hits = scan::Scanner().scan(*image, patterns);

	for (std::size_t i = 0; i < healthy.size(); ++i)
		CHECK_MESSAGE(hits[i].size() == 1, to_string(healthy[i].id));
}
