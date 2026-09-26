#include "rml/dumper/image/image.hpp"
#include "support/pe_builder.hpp"
#include "target/call_path.hpp"
#include "target/composite_anchor_resolver.hpp"
#include "target/text_anchor_resolver.hpp"

#include <array>
#include <cstring>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using namespace rml::dumper;
using namespace rml::dumper::target;

static constexpr Rva text_base = 0x1000;
static constexpr Rva data_base = 0x2000;
static constexpr Rva pdata_base = 0x3000;
static constexpr std::size_t exception_directory = 3;

static constexpr Rva caller = text_base;
static constexpr Rva first_callee = text_base + 0x100;
static constexpr Rva second_callee = text_base + 0x180;
static constexpr Rva leaf = text_base + 0x240;
static constexpr Rva misaligned = text_base + 0x263;

static void write_rip_lea(std::vector<std::byte>& code, const std::size_t offset, const Rva target)
{
	const auto next = static_cast<std::int64_t>(text_base + offset + 7);
	const auto displacement = static_cast<std::int32_t>(static_cast<std::int64_t>(target) - next);

	code[offset + 0] = std::byte{0x48};
	code[offset + 1] = std::byte{0x8D};
	code[offset + 2] = std::byte{0x15};
	std::memcpy(code.data() + offset + 3, &displacement, sizeof(displacement));
}

static void write_call(std::vector<std::byte>& code, const std::size_t offset, const Rva target)
{
	const auto next = static_cast<std::int64_t>(text_base + offset + 5);
	const auto displacement = static_cast<std::int32_t>(static_cast<std::int64_t>(target) - next);

	code[offset] = std::byte{0xE8};
	std::memcpy(code.data() + offset + 1, &displacement, sizeof(displacement));
}

static image::Image build()
{
	std::vector<std::byte> code(0x300, std::byte{0x90});
	write_rip_lea(code, 0x40, data_base + 1);
	write_call(code, 0x50, first_callee);
	write_call(code, 0x60, second_callee);
	write_call(code, 0x70, leaf);
	write_call(code, 0x80, misaligned);
	write_call(code, 0x110, second_callee);
	code[leaf - text_base - 1] = std::byte{0xCC};
	code[misaligned - text_base - 1] = std::byte{0xCC};

	static constexpr char contents[] = "\0hello\0";
	std::vector<std::byte> pool(sizeof(contents) - 1);
	std::memcpy(pool.data(), contents, pool.size());

	const std::array<std::pair<Rva, Rva>, 3> functions{{{caller, first_callee}, {first_callee, second_callee}, {second_callee, text_base + 0x200}}};

	tests::PeBuilder builder;
	builder.add_section(".text", text_base, code);
	builder.add_data_section(".rdata", data_base, pool);

	const auto table = tests::PeBuilder::runtime_function_table(functions);
	builder.add_data_section(".pdata", pdata_base, table);
	builder.set_directory(exception_directory, pdata_base, static_cast<std::uint32_t>(table.size()));

	auto image = image::ImageLoader::load(builder.write_to_temporary_file(), Architecture::x86_64);
	REQUIRE(image.has_value());

	return std::move(*image);
}

TEST_CASE("calls reach listed functions and padded leaves but not arbitrary code")
{
	const auto image = build();
	const auto function = image.functions().at(caller);
	REQUIRE(function.has_value());

	const auto targets = calls_from(image, *function);
	REQUIRE(targets.size() == 3);
	CHECK(targets[0] == first_callee);
	CHECK(targets[1] == second_callee);
	CHECK(targets[2] == leaf);
}

TEST_CASE("a text anchor walks the calls of the function that references the literal")
{
	const auto image = build();

	const std::array<AnchorSpec, 2> anchors{{
	    {.id = Anchor::lua_settop, .text = "hello", .path = {{{Step::call, 1}}}},
	    {.id = Anchor::lua_toboolean, .text = "hello", .path = {{{Step::call_from_end, 0}}}},
	}};

	const auto resolved = TextAnchorResolver().resolve(image, anchors);
	REQUIRE(resolved.has_value());
	CHECK(resolved->at(Anchor::lua_settop) == second_callee);
	CHECK(resolved->at(Anchor::lua_toboolean) == leaf);
}

TEST_CASE("an anchor can continue from another anchor")
{
	const auto image = build();

	const std::array<AnchorSpec, 2> anchors{{
	    {.id = Anchor::lua_getinfo, .text = "hello"},
	    {.id = Anchor::lua_resume, .origin = Anchor::lua_getinfo, .path = {{{Step::call, 0}, {Step::call, 0}}}},
	}};

	const auto resolved = CompositeAnchorResolver().resolve(image, anchors);
	const std::string reason = resolved.has_value() ? std::string{} : resolved.error().message();
	INFO(reason);
	REQUIRE(resolved.has_value());
	CHECK(resolved->at(Anchor::lua_getinfo) == caller);
	CHECK(resolved->at(Anchor::lua_resume) == second_callee);
}

TEST_CASE("a path past the last call is refused with the number of calls")
{
	const auto image = build();

	const std::array<AnchorSpec, 1> anchors{{{.id = Anchor::lua_settop, .text = "hello", .path = {{{Step::call, 5}}}}}};

	const auto resolved = TextAnchorResolver().resolve(image, anchors);
	REQUIRE_FALSE(resolved.has_value());
	CHECK(resolved.error().message().find("has 3 calls") != std::string::npos);
}
