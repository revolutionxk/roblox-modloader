#include <doctest/doctest.h>

#include "rml/dumper/scan/pattern.hpp"
#include "rml/dumper/target/target_profile.hpp"
#include "support/pe_builder.hpp"
#include "target/pattern_anchor_resolver.hpp"

#include <algorithm>

using namespace rml::dumper;
using namespace rml::dumper::target;

static image::Image image_with_code(std::vector<std::byte> code)
{
	tests::PeBuilder builder;
	builder.add_section(".text", 0x1000, code);

	auto image = image::ImageLoader::load(builder.write_to_temporary_file(), Architecture::x86_64);
	REQUIRE(image.has_value());

	return std::move(*image);
}

TEST_CASE("the three targets are registered")
{
	CHECK(TargetRegistry::find("windows-x64") != nullptr);
	CHECK(TargetRegistry::find("macos-x64") != nullptr);
	CHECK(TargetRegistry::find("macos-arm64") != nullptr);
	CHECK(TargetRegistry::find("solaris-sparc") == nullptr);
	CHECK(TargetRegistry::all().size() == 3);
	CHECK(TargetRegistry::names().find("macos-arm64") != std::string::npos);
}

TEST_CASE("the windows target is fully specified")
{
	const auto* profile = TargetRegistry::find("windows-x64");
	REQUIRE(profile != nullptr);

	CHECK(profile->format == ImageFormat::pe);
	CHECK(profile->architecture == Architecture::x86_64);
	CHECK(profile->abi->argument(0) == disasm::Register::rcx);
	CHECK(profile->anchors.size() == anchor_count);

	for (const auto& spec : profile->anchors)
	{
		const bool locatable = !spec.pattern.empty() || !spec.text.empty() || spec.origin.has_value();
		CHECK_MESSAGE(locatable, to_string(spec.id));

		if (spec.pattern.empty())
			continue;

		const auto pattern = scan::Pattern::parse(spec.pattern);
		CHECK_MESSAGE(pattern.has_value(), to_string(spec.id));
	}
}

TEST_CASE("anchors located by a literal declare no pattern")
{
	const auto* profile = TargetRegistry::find("windows-x64");
	REQUIRE(profile != nullptr);

	for (const auto& spec : profile->anchors)
		if (!spec.text.empty())
			CHECK_MESSAGE(spec.pattern.empty(), to_string(spec.id));
}

TEST_CASE("every anchor appears exactly once in the windows profile")
{
	const auto* profile = TargetRegistry::find("windows-x64");
	REQUIRE(profile != nullptr);

	for (std::size_t i = 0; i < anchor_count; ++i)
	{
		const auto id = static_cast<Anchor>(i);
		const auto count = std::ranges::count(profile->anchors, id, &AnchorSpec::id);
		CHECK_MESSAGE(count == 1, to_string(id));
	}
}

TEST_CASE("the macos targets carry no anchors yet")
{
	CHECK(TargetRegistry::find("macos-x64")->anchors.empty());
	CHECK(TargetRegistry::find("macos-arm64")->anchors.empty());
	CHECK(TargetRegistry::find("macos-x64")->format == ImageFormat::mach_o);
	CHECK(TargetRegistry::find("macos-arm64")->architecture == Architecture::arm64);
	CHECK(TargetRegistry::find("macos-arm64")->abi->argument(0) == disasm::Register::x0);
}

TEST_CASE("the resolver maps every anchor to an address")
{
	std::vector<std::byte> code(0x400, std::byte{0x90});
	code[0x40] = std::byte{0x11};
	code[0x41] = std::byte{0x22};
	code[0x42] = std::byte{0x33};
	code[0x80] = std::byte{0x44};
	code[0x81] = std::byte{0x55};
	code[0x82] = std::byte{0x66};

	const auto image = image_with_code(code);

	const std::array specs{AnchorSpec{Anchor::luaD_reallocstack, "11 22 33"},
	                       AnchorSpec{Anchor::luaH_new, "44 ? 66"}};

	const auto anchors = PatternAnchorResolver().resolve(image, specs);

	REQUIRE(anchors.has_value());
	CHECK(anchors->at(Anchor::luaD_reallocstack) == 0x1040);
	CHECK(anchors->at(Anchor::luaH_new) == 0x1080);
	CHECK(anchors->has(Anchor::luaH_new));
	CHECK_FALSE(anchors->has(Anchor::luaU_load));
}

TEST_CASE("the resolver fails when an anchor is missing")
{
	const auto image = image_with_code(std::vector<std::byte>(0x100, std::byte{0x90}));

	const std::array specs{AnchorSpec{Anchor::luaU_load, "DE AD BE EF"}};

	const auto anchors = PatternAnchorResolver().resolve(image, specs);

	REQUIRE_FALSE(anchors.has_value());
	CHECK(anchors.error().code() == ErrorCode::anchor);
	CHECK(anchors.error().message().find("luaU_load") != std::string::npos);
	CHECK(anchors.error().message().find("no match") != std::string::npos);
}

TEST_CASE("the resolver fails when an anchor is ambiguous")
{
	std::vector<std::byte> code(0x400, std::byte{0x90});
	code[0x40] = std::byte{0x11};
	code[0x41] = std::byte{0x22};
	code[0x80] = std::byte{0x11};
	code[0x81] = std::byte{0x22};

	const auto image = image_with_code(code);

	const std::array specs{AnchorSpec{Anchor::luaM_free, "11 22"}};

	const auto anchors = PatternAnchorResolver().resolve(image, specs);

	REQUIRE_FALSE(anchors.has_value());
	CHECK(anchors.error().message().find("2 matches") != std::string::npos);
	CHECK(anchors.error().message().find("luaM_free") != std::string::npos);
}

TEST_CASE("the resolver reports every failing anchor at once")
{
	const auto image = image_with_code(std::vector<std::byte>(0x100, std::byte{0x90}));

	const std::array specs{AnchorSpec{Anchor::luaU_load, "DE AD"}, AnchorSpec{Anchor::luaH_new, "BE EF"},
	                       AnchorSpec{Anchor::lua_resume, "CA FE"}};

	const auto anchors = PatternAnchorResolver().resolve(image, specs);

	REQUIRE_FALSE(anchors.has_value());
	CHECK(anchors.error().message().find("luaU_load") != std::string::npos);
	CHECK(anchors.error().message().find("luaH_new") != std::string::npos);
	CHECK(anchors.error().message().find("lua_resume") != std::string::npos);
	CHECK(anchors.error().message().find("3 of 3") != std::string::npos);
}

TEST_CASE("the resolver refuses a profile with no anchors")
{
	const auto image = image_with_code(std::vector<std::byte>(0x100, std::byte{0x90}));

	const auto anchors = PatternAnchorResolver().resolve(image, {});

	REQUIRE_FALSE(anchors.has_value());
	CHECK(anchors.error().code() == ErrorCode::anchor);
}
